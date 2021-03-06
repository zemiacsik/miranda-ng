/*

Copyright (C) 2009 Ricardo Pescuma Domenecci
Copyright (C) 2012-18 Miranda NG team

This is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with this file; see the file license.txt.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.
*/

#include "stdafx.h"

#include "m_cluiframes.h"

#include "extraicons.h"
#include "usedIcons.h"
#include "clc.h"

// Prototypes ///////////////////////////////////////////////////////////////////////////

int SortFunc(const ExtraIcon *p1, const ExtraIcon *p2)
{
	int ret = p1->getPosition() - p2->getPosition();
	if (ret != 0)
		return ret;

	int id1 = (p1->getType() != EXTRAICON_TYPE_GROUP) ? ((BaseExtraIcon*)p1)->getID() : 0;
	int id2 = (p2->getType() != EXTRAICON_TYPE_GROUP) ? ((BaseExtraIcon*)p2)->getID() : 0;
	return id1 - id2;
}

LIST<ExtraIcon> extraIconsByHandle(10), extraIconsBySlot(10, SortFunc);
LIST<BaseExtraIcon> registeredExtraIcons(10);

BOOL clistRebuildAlreadyCalled = FALSE;
BOOL clistApplyAlreadyCalled = FALSE;

// Functions ////////////////////////////////////////////////////////////////////////////

int InitOptionsCallback(WPARAM wParam, LPARAM lParam);

// Called when all the modules are loaded
int ModulesLoaded(WPARAM, LPARAM)
{
	HookEvent(ME_OPT_INITIALISE, InitOptionsCallback);
	return 0;
}

int GetNumberOfSlots()
{
	return EXTRA_ICON_COUNT;
}

int ConvertToClistSlot(int slot)
{
	if (slot < 0)
		return slot;

	return slot + 1;
}

int ExtraImage_ExtraIDToColumnNum(int extra)
{
	return (extra < 1 || extra > EXTRA_ICON_COUNT) ? -1 : extra - 1;
}

int Clist_SetExtraIcon(MCONTACT hContact, int slot, HANDLE hImage)
{
	if (cli.hwndContactTree == nullptr)
		return -1;

	int icol = ExtraImage_ExtraIDToColumnNum(ConvertToClistSlot(slot));
	if (icol == -1)
		return -1;

	HANDLE hItem = (HANDLE)SendMessage(cli.hwndContactTree, CLM_FINDCONTACT, hContact, 0);
	if (hItem == nullptr)
		return -1;

	SendMessage(cli.hwndContactTree, CLM_SETEXTRAIMAGE, (WPARAM)hItem, MAKELPARAM(icol, hImage));
	return 0;
}

ExtraIcon* GetExtraIcon(HANDLE id)
{
	int i = (INT_PTR)id;
	if (i < 1 || i > extraIconsByHandle.getCount())
		return nullptr;

	return extraIconsByHandle[i - 1];
}

ExtraIcon* GetExtraIconBySlot(int slot)
{
	for (int i = 0; i < extraIconsBySlot.getCount(); i++) {
		ExtraIcon *extra = extraIconsBySlot[i];
		if (extra->getSlot() == slot)
			return extra;
	}
	return nullptr;
}

BaseExtraIcon* GetExtraIconByName(const char *name)
{
	for (int i = 0; i < registeredExtraIcons.getCount(); i++) {
		BaseExtraIcon *extra = registeredExtraIcons[i];
		if (mir_strcmp(name, extra->getName()) == 0)
			return extra;
	}
	return nullptr;
}

static void LoadGroups(LIST<ExtraIconGroup> &groups)
{
	int count = db_get_w(0, MODULE_NAME "Groups", "Count", 0);
	for (int i = 0; i < count; i++) {
		char setting[512];
		mir_snprintf(setting, "%d_count", i);
		unsigned int items = db_get_w(0, MODULE_NAME "Groups", setting, 0);
		if (items < 1)
			continue;

		mir_snprintf(setting, "__group_%d", i);
		ExtraIconGroup *group = new ExtraIconGroup(setting);

		for (unsigned int j = 0; j < items; j++) {
			mir_snprintf(setting, "%d_%d", i, j);
			ptrA szIconName(db_get_sa(0, MODULE_NAME "Groups", setting));
			if (IsEmpty(szIconName))
				continue;

			BaseExtraIcon *extra = GetExtraIconByName(szIconName);
			if (extra == nullptr)
				continue;

			group->m_items.insert(extra);
			if (extra->getSlot() >= 0)
				group->setSlot(extra->getSlot());
		}

		if (group->m_items.getCount() < 2) {
			delete group;
			continue;
		}

		groups.insert(group);
	}
}

static ExtraIconGroup* IsInGroup(LIST<ExtraIconGroup> &groups, BaseExtraIcon *extra)
{
	for (int i = 0; i < groups.getCount(); i++) {
		ExtraIconGroup *group = groups[i];
		for (int j = 0; j < group->m_items.getCount(); j++) {
			if (extra == group->m_items[j])
				return group;
		}
	}
	return nullptr;
}

void RebuildListsBasedOnGroups(LIST<ExtraIconGroup> &groups)
{
	extraIconsByHandle.destroy();

	for (int i = 0; i < registeredExtraIcons.getCount(); i++)
		extraIconsByHandle.insert(registeredExtraIcons[i]);

	for (int k = 0; k < extraIconsBySlot.getCount(); k++) {
		ExtraIcon *extra = extraIconsBySlot[k];
		if (extra->getType() == EXTRAICON_TYPE_GROUP)
			delete extra;
	}
	extraIconsBySlot.destroy();

	for (int i = 0; i < groups.getCount(); i++) {
		ExtraIconGroup *group = groups[i];

		for (int j = 0; j < group->m_items.getCount(); j++)
			extraIconsByHandle.put(group->m_items[j]->getID() - 1, group);

		extraIconsBySlot.insert(group);
	}

	for (int k = 0; k < extraIconsByHandle.getCount(); k++) {
		ExtraIcon *extra = extraIconsByHandle[k];
		if (extra->getType() != EXTRAICON_TYPE_GROUP)
			extraIconsBySlot.insert(extra);
	}
}

///////////////////////////////////////////////////////////////////////////////

MIR_APP_DLL(void) KillModuleExtraIcons(int _hLang)
{
	LIST<ExtraIcon> arDeleted(1);

	for (int i = registeredExtraIcons.getCount() - 1; i >= 0; i--) {
		BaseExtraIcon *p = registeredExtraIcons[i];
		if (p->m_hLangpack == _hLang) {
			registeredExtraIcons.remove(i);
			arDeleted.insert(p);
		}
	}

	if (arDeleted.getCount() == 0)
		return;

	LIST<ExtraIconGroup> groups(1);
	LoadGroups(groups);
	RebuildListsBasedOnGroups(groups);

	for (int k = 0; k < arDeleted.getCount(); k++)
		delete arDeleted[k];
}

///////////////////////////////////////////////////////////////////////////////

int ClistExtraListRebuild(WPARAM, LPARAM)
{
	clistRebuildAlreadyCalled = TRUE;

	ResetIcons();

	for (int i = 0; i < extraIconsBySlot.getCount(); i++)
		extraIconsBySlot[i]->rebuildIcons();

	return 0;
}

int ClistExtraImageApply(WPARAM hContact, LPARAM)
{
	if (hContact == 0)
		return 0;

	clistApplyAlreadyCalled = TRUE;

	for (int i = 0; i < extraIconsBySlot.getCount(); i++)
		extraIconsBySlot[i]->applyIcon(hContact);

	return 0;
}

int ClistExtraClick(WPARAM hContact, LPARAM lParam)
{
	if (hContact == 0)
		return 0;

	int clistSlot = (int)lParam;

	for (int i = 0; i < extraIconsBySlot.getCount(); i++) {
		ExtraIcon *extra = extraIconsBySlot[i];
		if (ConvertToClistSlot(extra->getSlot()) == clistSlot) {
			extra->onClick(hContact);
			break;
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Extra image list functions

HANDLE hEventExtraImageListRebuilding, hEventExtraImageApplying, hEventExtraClick;

static bool bImageCreated = false;
static HIMAGELIST hExtraImageList;

MIR_APP_DLL(HANDLE) ExtraIcon_AddIcon(HICON hIcon)
{
	if (hExtraImageList == nullptr || hIcon == nullptr)
		return INVALID_HANDLE_VALUE;

	int res = ImageList_AddIcon(hExtraImageList, hIcon);
	return (res > 0xFFFE) ? INVALID_HANDLE_VALUE : (HANDLE)res;
}

void fnReloadExtraIcons()
{
	SendMessage(cli.hwndContactTree, CLM_SETEXTRASPACE, db_get_b(0, "CLUI", "ExtraColumnSpace", 18), 0);
	SendMessage(cli.hwndContactTree, CLM_SETEXTRAIMAGELIST, 0, 0);

	if (hExtraImageList)
		ImageList_Destroy(hExtraImageList);

	hExtraImageList = ImageList_Create(g_iIconSX, g_iIconSY, ILC_COLOR32 | ILC_MASK, 1, 256);

	SendMessage(cli.hwndContactTree, CLM_SETEXTRAIMAGELIST, 0, (LPARAM)hExtraImageList);
	SendMessage(cli.hwndContactTree, CLM_SETEXTRACOLUMNS, EXTRA_ICON_COUNT, 0);
	NotifyEventHooks(hEventExtraImageListRebuilding, 0, 0);
	bImageCreated = true;
}

void fnSetAllExtraIcons(MCONTACT hContact)
{
	if (cli.hwndContactTree == nullptr)
		return;

	bool hcontgiven = (hContact != 0);

	if (!bImageCreated)
		cli.pfnReloadExtraIcons();

	SendMessage(cli.hwndContactTree, CLM_SETEXTRACOLUMNS, EXTRA_ICON_COUNT, 0);

	if (hContact == 0)
		hContact = db_find_first();

	for (; hContact; hContact = db_find_next(hContact)) {
		NotifyEventHooks(hEventExtraImageApplying, hContact, 0);
		if (hcontgiven)
			break;
	}

	cli.pfnInvalidateRect(cli.hwndContactTree, nullptr, FALSE);
}

///////////////////////////////////////////////////////////////////////////////
// external functions

static void EI_PostCreate(BaseExtraIcon *extra, const char *name, int flags, int _hLang)
{
	char setting[512];
	mir_snprintf(setting, "Position_%s", name);
	extra->setPosition(db_get_w(0, MODULE_NAME, setting, 1000));

	mir_snprintf(setting, "Slot_%s", name);
	int slot = db_get_w(0, MODULE_NAME, setting, -100);
	if (slot == EMPTY_EXTRA_ICON)
		slot = -1;
	else if (slot == -100) {
		if (flags & EIF_DISABLED_BY_DEFAULT) {
			db_set_w(0, MODULE_NAME, setting, EMPTY_EXTRA_ICON);
			slot = -1;
		}
		else slot = 1;
	}
	extra->setSlot(slot);

	extra->m_hLangpack = _hLang;

	registeredExtraIcons.insert(extra);
	extraIconsByHandle.insert(extra);

	LIST<ExtraIconGroup> groups(1);
	LoadGroups(groups);

	ExtraIconGroup *group = IsInGroup(groups, extra);
	if (group != nullptr)
		RebuildListsBasedOnGroups(groups);
	else {
		for (int i = 0; i < groups.getCount(); i++)
			delete groups[i];

		extraIconsBySlot.insert(extra);
	}

	if (slot >= 0 || group != nullptr) {
		if (clistRebuildAlreadyCalled)
			extra->rebuildIcons();

		slot = 0;
		for (int i = 0; i < extraIconsBySlot.getCount(); i++) {
			ExtraIcon *ex = extraIconsBySlot[i];
			if (ex->getSlot() < 0)
				continue;

			int oldSlot = ex->getSlot();
			ex->setSlot(slot++);

			if (clistApplyAlreadyCalled && (ex == group || ex == extra || oldSlot != slot))
				extra->applyIcons();
		}
	}
}

EXTERN_C MIR_APP_DLL(HANDLE) ExtraIcon_RegisterCallback(const char *name, const char *description, const char *descIcon,
	MIRANDAHOOK RebuildIcons, MIRANDAHOOK ApplyIcon,
	MIRANDAHOOKPARAM OnClick, LPARAM onClickParam, int flags, int _hLang)
{
	// EXTRAICON_TYPE_CALLBACK 
	if (IsEmpty(name) || IsEmpty(description))
		return nullptr;

	if (ApplyIcon == nullptr || RebuildIcons == nullptr)
		return nullptr;

	// no way to merge
	if (GetExtraIconByName(name) != nullptr)
		return nullptr;

	ptrW tszDesc(mir_a2u(description));
	wchar_t *desc = TranslateW_LP(tszDesc, _hLang);

	int id = registeredExtraIcons.getCount() + 1;
	BaseExtraIcon *extra = new CallbackExtraIcon(id, name, desc, descIcon == nullptr ? "" : descIcon, RebuildIcons, ApplyIcon, OnClick, onClickParam);
	EI_PostCreate(extra, name, flags, _hLang);
	return (HANDLE)id;
}

EXTERN_C MIR_APP_DLL(HANDLE) ExtraIcon_RegisterIcolib(const char *name, const char *description, const char *descIcon,
	MIRANDAHOOKPARAM OnClick, LPARAM onClickParam, int flags, int _hLang)
{
	if (IsEmpty(name) || IsEmpty(description))
		return nullptr;

	ptrW tszDesc(mir_a2u(description));
	wchar_t *desc = TranslateW_LP(tszDesc, _hLang);

	BaseExtraIcon *extra = GetExtraIconByName(name);
	if (extra != nullptr) {
		if (extra->getType() != EXTRAICON_TYPE_ICOLIB)
			return nullptr;

		// Found one, now merge it
		if (mir_wstrcmpi(extra->getDescription(), desc)) {
			CMStringW newDesc = extra->getDescription();
			newDesc += L" / ";
			newDesc += desc;
			extra->setDescription(newDesc.c_str());
		}

		if (!IsEmpty(descIcon))
			extra->setDescIcon(descIcon);

		if (OnClick != nullptr)
			extra->setOnClick(OnClick, onClickParam);

		if (extra->getSlot() > 0) {
			if (clistRebuildAlreadyCalled)
				extra->rebuildIcons();
			if (clistApplyAlreadyCalled)
				extraIconsByHandle[extra->getID() - 1]->applyIcons();
		}

		return (HANDLE)extra->getID();
	}

	int id = registeredExtraIcons.getCount() + 1;
	extra = new IcolibExtraIcon(id, name, desc, descIcon == nullptr ? "" : descIcon, OnClick, onClickParam);
	EI_PostCreate(extra, name, flags, _hLang);
	return (HANDLE)id;
}

///////////////////////////////////////////////////////////////////////////////

MIR_APP_DLL(int) ExtraIcon_SetIcon(HANDLE hExtraIcon, MCONTACT hContact, HANDLE hImage)
{
	if (hExtraIcon == nullptr || hContact == 0)
		return -1;

	ExtraIcon *extra = GetExtraIcon(hExtraIcon);
	if (extra == nullptr)
		return -1;

	return extra->setIcon((INT_PTR)hExtraIcon, hContact, hImage);
}

MIR_APP_DLL(int) ExtraIcon_SetIconByName(HANDLE hExtraIcon, MCONTACT hContact, const char *icoName)
{
	if (hExtraIcon == nullptr || hContact == 0)
		return -1;

	ExtraIcon *extra = GetExtraIcon(hExtraIcon);
	if (extra == nullptr)
		return -1;

	return extra->setIconByName((INT_PTR)hExtraIcon, hContact, icoName);
}

MIR_APP_DLL(int) ExtraIcon_Clear(HANDLE hExtraIcon, MCONTACT hContact)
{
	if (hExtraIcon == nullptr || hContact == 0)
		return -1;

	ExtraIcon *extra = GetExtraIcon(hExtraIcon);
	if (extra == nullptr)
		return -1;

	return extra->setIcon((INT_PTR)hExtraIcon, hContact, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

static IconItem iconList[] =
{
	{ LPGEN("Chat activity"), "ChatActivity",  IDI_CHAT   },
	{ LPGEN("Male"),          "gender_male",   IDI_MALE   },
	{ LPGEN("Female"),		  "gender_female", IDI_FEMALE }
};

void LoadExtraIconsModule()
{
	// Events
	hEventExtraClick = CreateHookableEvent(ME_CLIST_EXTRA_CLICK);
	hEventExtraImageApplying = CreateHookableEvent(ME_CLIST_EXTRA_IMAGE_APPLY);
	hEventExtraImageListRebuilding = CreateHookableEvent(ME_CLIST_EXTRA_LIST_REBUILD);

	// Icons
	Icon_Register(g_hInst, LPGEN("Contact list"), iconList, _countof(iconList));

	// Hooks
	HookEvent(ME_SYSTEM_MODULESLOADED, ModulesLoaded);

	HookEvent(ME_CLIST_EXTRA_LIST_REBUILD, ClistExtraListRebuild);
	HookEvent(ME_CLIST_EXTRA_IMAGE_APPLY, ClistExtraImageApply);
	HookEvent(ME_CLIST_EXTRA_CLICK, ClistExtraClick);

	DefaultExtraIcons_Load();
}

void UnloadExtraIconsModule(void)
{
	for (int k = 0; k < extraIconsBySlot.getCount(); k++) {
		ExtraIcon *extra = extraIconsBySlot[k];
		if (extra->getType() == EXTRAICON_TYPE_GROUP)
			delete extra;
	}

	for (int i = 0; i < registeredExtraIcons.getCount(); i++)
		delete registeredExtraIcons[i];
}
