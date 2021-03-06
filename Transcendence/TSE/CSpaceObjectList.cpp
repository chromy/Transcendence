//	CSpaceObjectList.cpp
//
//	CSpaceObjectList class

#include "PreComp.h"

#define ALLOC_GRANULARITY				64

CSpaceObjectList::CSpaceObjectList (void)

//	CSpaceObjectList constructor

	{
	}

CSpaceObjectList::~CSpaceObjectList (void)

//	CSpaceObjectList destructor

	{
	}

void CSpaceObjectList::Add (CSpaceObject *pObj, int *retiIndex)

//	Add
//
//	Add object to the list. If the object is already in the list, we
//	don't add it.

	{
	if (FindObj(pObj))
		return;

	FastAdd(pObj, retiIndex);
	}

void CSpaceObjectList::ReadFromStream (SLoadCtx &Ctx)

//	ReadFromStream
//
//	Read the list from a stream

	{
	ASSERT(m_List.GetCount() == 0);

	DWORD dwCount;
	Ctx.pStream->Read((char *)&dwCount, sizeof(DWORD));
	if (dwCount)
		{
		m_List.InsertEmpty(dwCount);

		for (int i = 0; i < (int)dwCount; i++)
			Ctx.pSystem->ReadObjRefFromStream(Ctx, &m_List[i]);
		}
	}

bool CSpaceObjectList::Remove (CSpaceObject *pObj)

//	Remove
//
//	Remove the object. Returns TRUE if we removed the object.

	{
	int iIndex;
	if (FindObj(pObj, &iIndex))
		{
		Remove(iIndex);
		return true;
		}

	return false;
	}

void CSpaceObjectList::SetAllocSize (int iNewCount)

//	SetAllocSize
//
//	Sets the size of the buffer in preparation for adding objects.
//	NOTE that this also empties the list.

	{
	int iNeeded = (iNewCount - m_List.GetCount());
	if (iNeeded > 0)
		m_List.InsertEmpty(iNeeded);

	m_List.DeleteAll();
	}

void CSpaceObjectList::Subtract (const CSpaceObjectList &List)

//	Subtract
//
//	Removes all objects in List from the current list

	{
	int i;

	//	Mark all current objects

	int iCount = GetCount();
	for (i = 0; i < iCount; i++)
		GetObj(i)->SetMarked(true);

	//	Clear marks on all objects to remove

	for (i = 0; i < List.GetCount(); i++)
		List.GetObj(i)->SetMarked(false);

	//	Create a new list with the remaining objects

	TArray<CSpaceObject *> NewList;
	for (i = 0; i < iCount; i++)
		if (GetObj(i)->IsMarked())
			NewList.Insert(GetObj(i));

	m_List.TakeHandoff(NewList);
	}

void CSpaceObjectList::WriteToStream (CSystem *pSystem, IWriteStream *pStream)

//	WriteToStream
//
//	Write list to stream

	{
	int iCount = GetCount();
	pStream->Write((char *)&iCount, sizeof(int));

	for (int i = 0; i < iCount; i++)
		pSystem->WriteObjRefToStream(m_List[i], pStream);
	}
