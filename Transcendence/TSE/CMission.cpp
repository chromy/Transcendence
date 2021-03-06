//	CMission.cpp
//
//	CMission class
//	Copyright (c) 2012 by Kronosaur Productions, LLC. All Rights Reserved.

#include "PreComp.h"

static CObjectClass<CMission>g_MissionClass(OBJID_CMISSION, NULL);

#define EVENT_ON_ACCEPTED						CONSTLIT("OnAccepted")
#define EVENT_ON_COMPLETED						CONSTLIT("OnCompleted")
#define EVENT_ON_DECLINED						CONSTLIT("OnDeclined")
#define EVENT_ON_REWARD							CONSTLIT("OnReward")
#define EVENT_ON_SET_PLAYER_TARGET				CONSTLIT("OnSetPlayerTarget")
#define EVENT_ON_STARTED						CONSTLIT("OnStarted")

#define PROPERTY_IS_ACTIVE						CONSTLIT("isActive")
#define PROPERTY_IS_COMPLETED					CONSTLIT("isCompleted")
#define PROPERTY_IS_DEBRIEFED					CONSTLIT("isDebriefed")
#define PROPERTY_IS_DECLINED					CONSTLIT("isDeclined")
#define PROPERTY_IS_FAILURE						CONSTLIT("isFailure")
#define PROPERTY_IS_INTRO_SHOWN					CONSTLIT("isIntroShown")
#define PROPERTY_IS_OPEN						CONSTLIT("isOpen")
#define PROPERTY_IS_RECORDED					CONSTLIT("isRecorded")
#define PROPERTY_IS_SUCCESS						CONSTLIT("isSuccess")
#define PROPERTY_IS_UNAVAILABLE					CONSTLIT("isUnavailable")
#define PROPERTY_NODE_ID						CONSTLIT("nodeID")
#define PROPERTY_OWNER_ID						CONSTLIT("ownerID")
#define PROPERTY_UNID							CONSTLIT("unid")

#define REASON_ACCEPTED							CONSTLIT("accepted")
#define REASON_DEBRIEFED						CONSTLIT("debriefed")
#define REASON_DESTROYED						CONSTLIT("destroyed")
#define REASON_FAILURE							CONSTLIT("failure")
#define REASON_IN_PROGRESS						CONSTLIT("inProgress")
#define REASON_NEW_SYSTEM						CONSTLIT("newSystem")
#define REASON_SUCCESS							CONSTLIT("success")

#define SPECIAL_OWNER_ID						CONSTLIT("ownerID:")

#define STATUS_OPEN								CONSTLIT("open")
#define STATUS_CLOSED							CONSTLIT("closed")
#define STATUS_ACCEPTED							CONSTLIT("accepted")
#define STATUS_PLAYER_SUCCESS					CONSTLIT("playerSuccess")
#define STATUS_PLAYER_FAILURE					CONSTLIT("playerFailure")
#define STATUS_SUCCESS							CONSTLIT("success")
#define STATUS_FAILURE							CONSTLIT("failure")

#define STR_A_REASON							CONSTLIT("aReason")

CMission::CMission (void) : CSpaceObject(&g_MissionClass)

//	CMission constructor

	{
	}

void CMission::CloseMission (void)

//	CloseMission
//
//	Closes a mission

	{
	//	Remove all subscriptions

	CSystem *pSystem = g_pUniverse->GetCurrentSystem();
	if (pSystem)
		RemoveAllEventSubscriptions(pSystem);

	//	Cancel all timer events

	g_pUniverse->CancelEvent(this);
	}

void CMission::CompleteMission (ECompletedReasons iReason)

//	CompleteMission
//
//	Complete the mission

	{
	if (IsCompleted())
		return;

	bool bIsPlayerMission = (m_iStatus == statusAccepted);

	//	Handle player missions differently

	if (bIsPlayerMission)
		{
		//	Mission failure

		if (iReason == completeFailure || iReason == completeDestroyed)
			{
			m_iStatus = statusPlayerFailure;

			//	Tell the player that we failed

			CSpaceObject *pPlayer = g_pUniverse->GetPlayer();
			if (pPlayer)
				{
				CString sMessage;
				if (!Translate(CONSTLIT("FailureMsg"), &sMessage))
					sMessage = CONSTLIT("Mission failed!");

				pPlayer->SendMessage(NULL, sMessage);
				}

			//	Set the player target (mission usually sets the target back to the 
			//	station that gave the mission).

			FireOnSetPlayerTarget(REASON_FAILURE);
			}

		//	Mission success

		else if (iReason == completeSuccess)
			{
			m_iStatus = statusPlayerSuccess;

			//	Tell the player that we succeeded

			CSpaceObject *pPlayer = g_pUniverse->GetPlayer();
			if (pPlayer)
				{
				CString sMessage;
				if (!Translate(CONSTLIT("SuccessMsg"), &sMessage))
					sMessage = CONSTLIT("Mission complete!");

				pPlayer->SendMessage(NULL, sMessage);
				}

			//	Set the player target (mission usually sets the target back to the 
			//	station that gave the mission).

			FireOnSetPlayerTarget(REASON_SUCCESS);
			}
		}

	//	Set status for non-player missions

	else
		{
		if (iReason == completeFailure || iReason == completeDestroyed)
			m_iStatus = statusFailure;
		else if (iReason == completeSuccess)
			m_iStatus = statusSuccess;

		//	For non-player missions we can close now. (For players we wait until
		//	debrief.)

		CloseMission();
		}
	}

ALERROR CMission::Create (CMissionType *pType,
						  CSpaceObject *pOwner,
						  ICCItem *pCreateData,
					      CMission **retpMission,
						  CString *retsError)

//	Create
//
//	Creates a new mission object. We return ERR_NOTFOUND if the mission could 
//	not be created because conditions do not allow it.

	{
	CMission *pMission;

	pMission = new CMission;
	if (pMission == NULL)
		{
		*retsError = CONSTLIT("Out of memory");
		return ERR_MEMORY;
		}

	pMission->m_pType = pType;

	//	Initialize

	pMission->m_iStatus = statusOpen;
	pMission->m_fIntroShown = false;
	pMission->m_fDeclined = false;
	pMission->m_fDebriefed = false;
	pMission->m_pOwner = pOwner;

	//	NodeID

	CTopologyNode *pNode = NULL;
	CSystem *pSystem = NULL;
	if ((pSystem = (pOwner ? pOwner->GetSystem() : g_pUniverse->GetCurrentSystem()))
			&& (pNode = pSystem->GetTopology()))
		pMission->m_sNodeID = pNode->GetID();

	//	Fire OnCreate

	pMission->m_fInOnCreate = true;

	CSpaceObject::SOnCreate OnCreate;
	OnCreate.pData = pCreateData;
	OnCreate.pOwnerObj = pOwner;
	pMission->FireOnCreate(OnCreate);

	pMission->m_fInOnCreate = false;

	//	If OnCreate destroyed the object then it means that the mission was not
	//	suitable. We return ERR_NOTFOUND

	if (pMission->IsDestroyed())
		{
		delete pMission;
		return ERR_NOTFOUND;
		}

	//	If we haven't subscribed to the owner, do it now

	if (pOwner && !pOwner->FindEventSubscriber(pMission))
		pOwner->AddEventSubscriber(pMission);

	//	Done

	if (retpMission)
		*retpMission = pMission;

	return NOERROR;
	}

void CMission::FireCustomEvent (const CString &sEvent)

//	FireCustomEvent
//
//	Fires a custom timed event

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(sEvent, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SetEvent(eventDoEvent);
		Ctx.SaveAndDefineSourceVar(this);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(sEvent, pResult);
		Ctx.Discard(pResult);
		}
	}

void CMission::FireOnAccepted (void)

//	FireOnAccepted
//
//	Fire <OnAccepted>

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(EVENT_ON_ACCEPTED, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SaveAndDefineSourceVar(this);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(EVENT_ON_ACCEPTED, pResult);
		Ctx.Discard(pResult);
		}
	}

void CMission::FireOnDeclined (void)

//	FireOnDeclined
//
//	Fire <OnDeclined>

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(EVENT_ON_DECLINED, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SaveAndDefineSourceVar(this);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(EVENT_ON_DECLINED, pResult);
		Ctx.Discard(pResult);
		}
	}

void CMission::FireOnReward (ICCItem *pData)

//	FireOnReward
//
//	Fire <OnReward>

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(EVENT_ON_REWARD, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SaveAndDefineSourceVar(this);
		Ctx.SaveAndDefineDataVar(pData);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(EVENT_ON_REWARD, pResult);
		Ctx.Discard(pResult);
		}
	}

void CMission::FireOnSetPlayerTarget (const CString &sReason)

//	FireOnSetPlayerTarget
//
//	Fire <OnSetPlayerTarget>

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(EVENT_ON_SET_PLAYER_TARGET, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SaveAndDefineSourceVar(this);
		Ctx.DefineString(STR_A_REASON, sReason);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(EVENT_ON_SET_PLAYER_TARGET, pResult);
		Ctx.Discard(pResult);
		}
	}

void CMission::FireOnStart (void)

//	FireOnStart
//
//	Fire <OnStarted>

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(EVENT_ON_STARTED, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SaveAndDefineSourceVar(this);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(EVENT_ON_STARTED, pResult);
		Ctx.Discard(pResult);
		}
	}

void CMission::FireOnStop (const CString &sReason, ICCItem *pData)

//	FireOnStop
//
//	Fire <OnCompleted>

	{
	SEventHandlerDesc Event;

	if (FindEventHandler(EVENT_ON_COMPLETED, &Event))
		{
		CCodeChainCtx Ctx;

		Ctx.SaveAndDefineSourceVar(this);
		Ctx.SaveAndDefineDataVar(pData);
		Ctx.DefineString(STR_A_REASON, sReason);

		ICCItem *pResult = Ctx.Run(Event);
		if (pResult->IsError())
			ReportEventError(EVENT_ON_COMPLETED, pResult);
		Ctx.Discard(pResult);
		}
	}

ICCItem *CMission::GetProperty (const CString &sName)

//	GetProperty
//
//	Returns a property

	{
	CCodeChain &CC = g_pUniverse->GetCC();

	if (strEquals(sName, PROPERTY_IS_ACTIVE))
		return CC.CreateBool(IsActive());

	else if (strEquals(sName, PROPERTY_IS_COMPLETED))
		return CC.CreateBool(IsCompleted());

	else if (strEquals(sName, PROPERTY_IS_DEBRIEFED))
		return CC.CreateBool(m_fDebriefed);

	else if (strEquals(sName, PROPERTY_IS_DECLINED))
		return CC.CreateBool(m_fDeclined);

	else if (strEquals(sName, PROPERTY_IS_FAILURE))
		return CC.CreateBool(IsFailure());

	else if (strEquals(sName, PROPERTY_IS_INTRO_SHOWN))
		return CC.CreateBool(m_fIntroShown);

	else if (strEquals(sName, PROPERTY_IS_OPEN))
		return CC.CreateBool(m_iStatus == statusOpen);

	else if (strEquals(sName, PROPERTY_IS_RECORDED))
		return CC.CreateBool(IsRecorded());

	else if (strEquals(sName, PROPERTY_IS_SUCCESS))
		return CC.CreateBool(IsSuccess());

	else if (strEquals(sName, PROPERTY_IS_UNAVAILABLE))
		return CC.CreateBool(IsUnavailable());

	else if (strEquals(sName, PROPERTY_NODE_ID))
		return (m_sNodeID.IsBlank() ? CC.CreateNil() : CC.CreateString(m_sNodeID));

	else if (strEquals(sName, PROPERTY_OWNER_ID))
		{
		if (m_pOwner.GetID() == OBJID_NULL)
			return CC.CreateNil();
		else
			return CC.CreateInteger(m_pOwner.GetID());
		}

	else if (strEquals(sName, PROPERTY_UNID))
		return CC.CreateInteger(m_pType->GetUNID());

	else
		return CSpaceObject::GetProperty(sName);
	}

bool CMission::HasSpecialAttribute (const CString &sAttrib) const

//	HasSpecialAttribute
//
//	Returns TRUE if object has the special attribute
//
//	NOTE: Subclasses may override this, but they must call the
//	base class if they do not handle the attribute.

	{
	if (strStartsWith(sAttrib, SPECIAL_OWNER_ID))
		{
		DWORD dwOwnerID = strToInt(strSubString(sAttrib, SPECIAL_OWNER_ID.GetLength()), 0);
		return (dwOwnerID == m_pOwner.GetID());
		}
	else
		return CSpaceObject::HasSpecialAttribute(sAttrib);
	}

bool CMission::MatchesCriteria (CSpaceObject *pSource, const SCriteria &Criteria)

//	MatchesCriteria
//
//	Returns TRUE if the given mission matches the criteria

	{
	int i;

	//	By status

	if (Criteria.bIncludeActive && !IsActive())
		return false;

	if (Criteria.bIncludeOpen && m_iStatus != statusOpen)
		return false;

	if (Criteria.bIncludeRecorded && !IsRecorded())
		return false;

	if (Criteria.bIncludeUnavailable && !IsUnavailable())
		return false;

	//	Owned by source

	if (Criteria.bOnlySourceOwner)
		{
		if (pSource)
			{
			if (pSource->GetID() != m_pOwner.GetID())
				return false;
			}
		else
			{
			if (m_pOwner.GetID() != OBJID_NULL)
				return false;
			}
		}

	//	Check required attributes

	for (i = 0; i < Criteria.AttribsRequired.GetCount(); i++)
		if (!HasAttribute(Criteria.AttribsRequired[i]))
			return false;

	//	Check attributes not allowed

	for (i = 0; i < Criteria.AttribsNotAllowed.GetCount(); i++)
		if (HasAttribute(Criteria.AttribsNotAllowed[i]))
			return false;

	//	Check special attribs required

	for (i = 0; i < Criteria.SpecialRequired.GetCount(); i++)
		if (!HasSpecialAttribute(Criteria.SpecialRequired[i]))
			return false;

	//	Check special attribs not allowed

	for (i = 0; i < Criteria.SpecialNotAllowed.GetCount(); i++)
		if (HasSpecialAttribute(Criteria.SpecialNotAllowed[i]))
			return false;

	//	Match

	return true;
	}

void CMission::OnDestroyed (SDestroyCtx &Ctx)

//	OnDestroyed
//
//	Mission is destroyed

	{
	if (m_fInOnCreate)
		return;

	//	If the mission is running then we need to stop

	if (m_iStatus == statusClosed || m_iStatus == statusAccepted)
		{
		FireOnStop(REASON_DESTROYED, NULL);

		CSpaceObject *pOwner = m_pOwner.GetObj();
		if (pOwner)
			pOwner->FireOnMissionCompleted(this, REASON_DESTROYED);
		}

	//	Make sure the mission is completed

	CompleteMission(completeDestroyed);

	//	Destroy the mission

	FireOnDestroy(Ctx);
	}

void CMission::OnNewSystem (CSystem *pSystem)

//	OnNewSystem
//
//	We are in a new system

	{
	//	Ignore any closed missions (completed missions)

	if (IsClosed())
		return;

	//	Resolve owner

	m_pOwner.Resolve();

	//	Clear any object references (because they might belong to a different
	//	system).

	ClearObjReferences();
	}

void CMission::OnObjDestroyedNotify (SDestroyCtx &Ctx)

//	OnObjDestroyedNotify
//
//	An object that we subscribe to has been destroyed

	{
	//	Fire events

	FireOnObjDestroyed(Ctx);

	//	If this is the owner then the mission fails

	if (Ctx.pObj->GetID() == m_pOwner.GetID())
		{
		//	Mission fails

		if (m_pType->FailureWhenOwnerDestroyed())
			{
			SetFailure(NULL);

			//	Since the owner is dead, we do not require a debrief

			if (IsActive())
				{
				m_fDebriefed = true;

				FireOnSetPlayerTarget(REASON_DEBRIEFED);
				CloseMission();
				}
			}

		//	Clear out owner pointer

		m_pOwner.CleanUp();
		}
	}

void CMission::OnPlayerEnteredSystem (void)

//	OnPlayerEnteredSystem
//
//	Player has entered the system

	{
	//	For active missions, fire event to reset player targets.

	FireOnSetPlayerTarget(REASON_NEW_SYSTEM);
	}

void CMission::OnReadFromStream (SLoadCtx &Ctx)

//	OnReadFromStream
//
//	Read from stream
//
//	DWORD		Mission type UNID
//	DWORD		Mission status
//	DWORD		CGlobalSpaceObject
//	CString		m_sNodeID
//	DWORD		Flags

	{
	DWORD dwLoad;

	Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	m_pType = CMissionType::AsType(g_pUniverse->FindDesignType(dwLoad));
	if (m_pType == NULL)
		throw CException(ERR_FAIL);

	Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	m_iStatus = (EStatus)dwLoad;

	m_pOwner.ReadFromStream(Ctx);
	m_sNodeID.ReadFromStream(Ctx.pStream);

	//	Flags

	Ctx.pStream->Read((char *)&dwLoad, sizeof(DWORD));
	m_fIntroShown =		((dwLoad & 0x00000001) ? true : false);
	m_fDeclined	=		((dwLoad & 0x00000002) ? true : false);
	m_fDebriefed =		((dwLoad & 0x00000004) ? true : false);
	m_fInOnCreate =		false;
	}

void CMission::OnWriteToStream (IWriteStream *pStream)

//	OnWriteToStream
//
//	Write to stream
//
//	DWORD		Mission type UNID
//	DWORD		Mission status
//	DWORD		Player status
//	DWORD		CGlobalSpaceObject
//	CString		m_sNodeID
//	DWORD		Flags

	{
	DWORD dwSave;

	dwSave = m_pType->GetUNID();
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	dwSave = (DWORD)m_iStatus;
	pStream->Write((char *)&dwSave, sizeof(DWORD));

	m_pOwner.WriteToStream(pStream);
	m_sNodeID.WriteToStream(pStream);

	//	Flags

	dwSave = 0;
	dwSave |= (m_fIntroShown ?		0x00000001 : 0);
	dwSave |= (m_fDeclined ?		0x00000002 : 0);
	dwSave |= (m_fDebriefed ?		0x00000004 : 0);
	pStream->Write((char *)&dwSave, sizeof(DWORD));
	}

bool CMission::ParseCriteria (const CString &sCriteria, SCriteria *retCriteria)

//	ParseCriteria
//
//	Parses criteria. Returns TRUE if successful.

	{
	//	Initialize

	*retCriteria = SCriteria();

	//	Parse

	char *pPos = sCriteria.GetPointer();
	while (*pPos != '\0')
		{
		switch (*pPos)
			{
			case '*':
				retCriteria->bIncludeOpen = true;
				retCriteria->bIncludeUnavailable = true;
				retCriteria->bIncludeActive = true;
				retCriteria->bIncludeRecorded = true;
				break;

			case 'a':
				retCriteria->bIncludeActive = true;
				break;

			case 'o':
				retCriteria->bIncludeOpen = true;
				break;

			case 'r':
				retCriteria->bIncludeRecorded = true;
				break;

			case 'u':
				retCriteria->bIncludeUnavailable = true;
				break;

			case 'S':
				retCriteria->bOnlySourceOwner = true;
				break;

			case '+':
			case '-':
				{
				bool bRequired = (*pPos == '+');
				bool bBinaryParam;
				CString sParam = ParseCriteriaParam(&pPos, false, &bBinaryParam);

				if (bRequired)
					{
					if (bBinaryParam)
						retCriteria->SpecialRequired.Insert(sParam);
					else
						retCriteria->AttribsRequired.Insert(sParam);
					}
				else
					{
					if (bBinaryParam)
						retCriteria->SpecialNotAllowed.Insert(sParam);
					else
						retCriteria->AttribsNotAllowed.Insert(sParam);
					}
				break;
				}
			}

		pPos++;
		}

	//	Make sure we include some missions

	if (!retCriteria->bIncludeUnavailable
			&& !retCriteria->bIncludeActive
			&& !retCriteria->bIncludeRecorded
			&& !retCriteria->bIncludeOpen)
		{
		retCriteria->bIncludeUnavailable = true;
		retCriteria->bIncludeActive = true;
		retCriteria->bIncludeRecorded = true;
		retCriteria->bIncludeOpen = true;
		}

	return true;
	}

bool CMission::Reward (ICCItem *pData)

//	Reward
//
//	Reward the player for a mission success

	{
	//	If we haven't yet called success, then do it now

	if (!IsCompleted())
		SetSuccess(NULL);

	//	Reward

	FireOnReward(pData);

	//	Set debriefed to true as a convenience

	m_fDebriefed = true;
	FireOnSetPlayerTarget(REASON_DEBRIEFED);
	CloseMission();

	//	Done

	return true;
	}

bool CMission::SetAccepted (void)

//	SetAccepted
//
//	Player accepts a mission

	{
	//	Must be available to player.

	if (m_iStatus != statusOpen)
		return false;

	//	Player accepts the mission

	FireOnAccepted();

	CSpaceObject *pOwner = m_pOwner.GetObj();
	if (pOwner)
		pOwner->FireOnMissionAccepted(this);

	//	If the above call changed anything, then we're done

	if (m_iStatus != statusOpen)
		return false;

	//	Player has accepted

	m_iStatus = statusAccepted;

	//	Start the mission

	FireOnStart();

	//	Set the player target

	FireOnSetPlayerTarget(REASON_ACCEPTED);

	return true;
	}

bool CMission::SetDeclined (void)

//	SetDeclined
//
//	Mission declined by player

	{
	//	Must be available to player.

	if (m_iStatus == statusOpen)
		return false;

	//	Player declines the mission

	FireOnDeclined();

	//	Set flag

	m_fDeclined = true;

	return true;
	}

bool CMission::SetFailure (ICCItem *pData)

//	SetFailure
//
//	Mission failed

	{
	//	Must be in the right state

	if (m_iStatus != statusAccepted && m_iStatus != statusClosed && m_iStatus != statusOpen)
		return false;

	//	Stop the mission

	if (m_iStatus != statusOpen)
		{
		FireOnStop(REASON_FAILURE, pData);

		CSpaceObject *pOwner = m_pOwner.GetObj();
		if (pOwner)
			pOwner->FireOnMissionCompleted(this, REASON_FAILURE);
		}

	//	Done

	CompleteMission(completeFailure);

	return true;
	}

bool CMission::SetSuccess (ICCItem *pData)

//	SetSuccess
//
//	Mission succeeded

	{
	//	Must be in the right state

	if (m_iStatus != statusAccepted && m_iStatus != statusClosed && m_iStatus != statusOpen)
		return false;

	//	Stop the mission

	if (m_iStatus != statusOpen)
		{
		FireOnStop(REASON_SUCCESS, pData);

		CSpaceObject *pOwner = m_pOwner.GetObj();
		if (pOwner)
			pOwner->FireOnMissionCompleted(this, REASON_SUCCESS);
		}

	//	Done

	CompleteMission(completeSuccess);

	return true;
	}

bool CMission::SetUnavailable (void)

//	SetUnavailable
//
//	Mission starting without player

	{
	//	Must be open

	if (m_iStatus == statusOpen)
		return false;

	//	No player

	m_iStatus = statusClosed;

	//	Start the mission

	FireOnStart();

	return true;
	}

bool CMission::SetPlayerTarget (void)

//	SetPlayerTarget
//
//	Calls <OnSetPlayerTarget>

	{
	if (m_iStatus != statusAccepted)
		return false;

	//	Call OnSetPlayerTarget

	FireOnSetPlayerTarget(REASON_IN_PROGRESS);

	//	Done

	return true;
	}

bool CMission::SetProperty (const CString &sName, ICCItem *pValue, CString *retsError)

//	SetProperty
//
//	Sets an object property

	{
	if (strEquals(sName, PROPERTY_IS_DEBRIEFED))
		{
		if (m_iStatus == statusPlayerSuccess || m_iStatus == statusPlayerFailure)
			{
			if (!pValue->IsNil())
				{
				if (!m_fDebriefed)
					{
					m_fDebriefed = true;

					FireOnSetPlayerTarget(REASON_DEBRIEFED);
					CloseMission();
					}
				}
			else
				m_fDebriefed = false;
			}
		}

	else if (strEquals(sName, PROPERTY_IS_DECLINED))
		{
		if (m_iStatus == statusOpen)
			m_fDeclined = !pValue->IsNil();
		}

	else if (strEquals(sName, PROPERTY_IS_INTRO_SHOWN))
		{
		if (m_iStatus == statusOpen)
			m_fIntroShown = !pValue->IsNil();
		}

	else
		return CSpaceObject::SetProperty(sName, pValue, retsError);

	return true;
	}

