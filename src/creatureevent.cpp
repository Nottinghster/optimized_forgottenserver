/**
 * The Forgotten Server - a free and open-source MMORPG server emulator
 * Copyright (C) 2019  Mark Samman <mark.samman@gmail.com>
 * Copyright (C) 2019-2021  Saiyans King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "otpch.h"

#include "creatureevent.h"
#include "tools.h"
#include "player.h"

CreatureEvents::CreatureEvents() :
	scriptInterface("CreatureScript Interface")
{
	scriptInterface.initState();
}

void CreatureEvents::clear(bool fromLua)
{
	for (auto it = creatureEvents.begin(); it != creatureEvents.end(); ) {
		if (fromLua == it->second.fromLua) {
			it = creatureEvents.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = loginEvents.begin(); it != loginEvents.end(); ) {
		if (fromLua == (*it).fromLua) {
			it = loginEvents.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = logoutEvents.begin(); it != logoutEvents.end(); ) {
		if (fromLua == (*it).fromLua) {
			it = logoutEvents.erase(it);
		} else {
			++it;
		}
	}
	for (auto it = advanceEvents.begin(); it != advanceEvents.end(); ) {
		if (fromLua == (*it).fromLua) {
			it = advanceEvents.erase(it);
		} else {
			++it;
		}
	}

	reInitState(fromLua);
}

LuaScriptInterface& CreatureEvents::getScriptInterface()
{
	return scriptInterface;
}

std::string CreatureEvents::getScriptBaseName() const
{
	return "creaturescripts";
}

Event_ptr CreatureEvents::getEvent(const std::string& nodeName)
{
	if (strcasecmp(nodeName.c_str(), "event") != 0) {
		return nullptr;
	}
	return Event_ptr(new CreatureEvent(&scriptInterface));
}

bool CreatureEvents::registerEvent(Event_ptr event, const pugi::xml_node&)
{
	CreatureEvent_ptr creatureEvent{static_cast<CreatureEvent*>(event.release())}; //event is guaranteed to be a CreatureEvent
	if (creatureEvent->getEventType() == CREATURE_EVENT_NONE) {
		std::cout << "Error: [CreatureEvents::registerEvent] Trying to register event without type!" << std::endl;
		return false;
	}

	if (creatureEvent->getEventType() == CREATURE_EVENT_LOGIN) {
		loginEvents.emplace_back(std::move(*creatureEvent));
		return true;
	} else if (creatureEvent->getEventType() == CREATURE_EVENT_LOGOUT) {
		logoutEvents.emplace_back(std::move(*creatureEvent));
		return true;
	} else if (creatureEvent->getEventType() == CREATURE_EVENT_ADVANCE) {
		advanceEvents.emplace_back(std::move(*creatureEvent));
		return true;
	} else {
		std::string name = creatureEvent->getName();
		auto result = creatureEvents.emplace(name, std::move(*creatureEvent));
		if (!result.second) {
			std::cout << "[Warning - CreatureEvents::registerLuaEvent] Duplicate registered event with name: " << name << std::endl;
		}
		return result.second;
	}
}

bool CreatureEvents::registerLuaEvent(CreatureEvent* event)
{
	CreatureEvent_ptr creatureEvent{ event };
	if (creatureEvent->getEventType() == CREATURE_EVENT_NONE) {
		std::cout << "Error: [CreatureEvents::registerLuaEvent] Trying to register event without type!" << std::endl;
		return false;
	}

	if (creatureEvent->getEventType() == CREATURE_EVENT_LOGIN) {
		loginEvents.emplace_back(std::move(*creatureEvent));
		return true;
	} else if (creatureEvent->getEventType() == CREATURE_EVENT_LOGOUT) {
		logoutEvents.emplace_back(std::move(*creatureEvent));
		return true;
	} else if (creatureEvent->getEventType() == CREATURE_EVENT_ADVANCE) {
		advanceEvents.emplace_back(std::move(*creatureEvent));
		return true;
	} else {
		std::string name = creatureEvent->getName();
		auto result = creatureEvents.emplace(name, std::move(*creatureEvent));
		if (!result.second) {
			std::cout << "[Warning - CreatureEvents::registerLuaEvent] Duplicate registered event with name: " << name << std::endl;
		}
		return result.second;
	}
}

CreatureEvent* CreatureEvents::getEventByName(const std::string& name, bool forceLoaded /*= true*/)
{
	auto it = creatureEvents.find(name);
	if (it != creatureEvents.end()) {
		if (!forceLoaded || it->second.isLoaded()) {
			return &it->second;
		}
	}
	return nullptr;
}

bool CreatureEvents::playerLogin(Player* player) const
{
	//fire global event if is registered
	for (const auto& it : loginEvents) {
		if (!it.executeOnLogin(player)) {
			return false;
		}
	}
	return true;
}

bool CreatureEvents::playerLogout(Player* player) const
{
	//fire global event if is registered
	for (const auto& it : logoutEvents) {
		if (!it.executeOnLogout(player)) {
			return false;
		}
	}
	return true;
}

bool CreatureEvents::playerAdvance(Player* player, skills_t skill, uint32_t oldLevel, uint32_t newLevel)
{
	//fire global event if is registered
	for (auto& it : advanceEvents) {
		if (!it.executeAdvance(player, skill, oldLevel, newLevel)) {
			return false;
		}
	}
	return true;
}

/////////////////////////////////////

CreatureEvent::CreatureEvent(LuaScriptInterface* interface) :
	Event(interface), type(CREATURE_EVENT_NONE), loaded(false) {}

bool CreatureEvent::configureEvent(const pugi::xml_node& node)
{
	// Name that will be used in monster xml files and
	// lua function to register events to reference this event
	pugi::xml_attribute nameAttribute = node.attribute("name");
	if (!nameAttribute) {
		std::cout << "[Error - CreatureEvent::configureEvent] Missing name for creature event" << std::endl;
		return false;
	}

	eventName = nameAttribute.as_string();

	pugi::xml_attribute typeAttribute = node.attribute("type");
	if (!typeAttribute) {
		std::cout << "[Error - CreatureEvent::configureEvent] Missing type for creature event: " << eventName << std::endl;
		return false;
	}

	std::string tmpStr = asLowerCaseString(typeAttribute.as_string());
	if (!tfs_strcmp(tmpStr.c_str(), "login")) {
		type = CREATURE_EVENT_LOGIN;
	} else if (!tfs_strcmp(tmpStr.c_str(), "logout")) {
		type = CREATURE_EVENT_LOGOUT;
	} else if (!tfs_strcmp(tmpStr.c_str(), "think")) {
		type = CREATURE_EVENT_THINK;
	} else if (!tfs_strcmp(tmpStr.c_str(), "preparedeath")) {
		type = CREATURE_EVENT_PREPAREDEATH;
	} else if (!tfs_strcmp(tmpStr.c_str(), "death")) {
		type = CREATURE_EVENT_DEATH;
	} else if (!tfs_strcmp(tmpStr.c_str(), "kill")) {
		type = CREATURE_EVENT_KILL;
	} else if (!tfs_strcmp(tmpStr.c_str(), "advance")) {
		type = CREATURE_EVENT_ADVANCE;
	} else if (!tfs_strcmp(tmpStr.c_str(), "modalwindow")) {
		type = CREATURE_EVENT_MODALWINDOW;
	} else if (!tfs_strcmp(tmpStr.c_str(), "textedit")) {
		type = CREATURE_EVENT_TEXTEDIT;
	} else if (!tfs_strcmp(tmpStr.c_str(), "healthchange")) {
		type = CREATURE_EVENT_HEALTHCHANGE;
	} else if (!tfs_strcmp(tmpStr.c_str(), "manachange")) {
		type = CREATURE_EVENT_MANACHANGE;
	} else if (!tfs_strcmp(tmpStr.c_str(), "extendedopcode")) {
		type = CREATURE_EVENT_EXTENDED_OPCODE;
	} else {
		std::cout << "[Error - CreatureEvent::configureEvent] Invalid type for creature event: " << eventName << std::endl;
		return false;
	}

	loaded = true;
	return true;
}

std::string CreatureEvent::getScriptEventName() const
{
	//Depending on the type script event name is different
	switch (type) {
		case CREATURE_EVENT_LOGIN:
			return "onLogin";

		case CREATURE_EVENT_LOGOUT:
			return "onLogout";

		case CREATURE_EVENT_THINK:
			return "onThink";

		case CREATURE_EVENT_PREPAREDEATH:
			return "onPrepareDeath";

		case CREATURE_EVENT_DEATH:
			return "onDeath";

		case CREATURE_EVENT_KILL:
			return "onKill";

		case CREATURE_EVENT_ADVANCE:
			return "onAdvance";

		case CREATURE_EVENT_MODALWINDOW:
			return "onModalWindow";

		case CREATURE_EVENT_TEXTEDIT:
			return "onTextEdit";

		case CREATURE_EVENT_HEALTHCHANGE:
			return "onHealthChange";

		case CREATURE_EVENT_MANACHANGE:
			return "onManaChange";

		case CREATURE_EVENT_EXTENDED_OPCODE:
			return "onExtendedOpcode";

		case CREATURE_EVENT_NONE:
		default:
			return std::string();
	}
}

void CreatureEvent::copyEvent(CreatureEvent* creatureEvent)
{
	scriptId = creatureEvent->scriptId;
	scriptInterface = creatureEvent->scriptInterface;
	scripted = creatureEvent->scripted;
	loaded = creatureEvent->loaded;
}

bool CreatureEvent::executeOnLogin(Player* player) const
{
	//onLogin(player)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeOnLogin] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);
	LuaScriptInterface::pushUserdata(L, player);
	LuaScriptInterface::setMetatable(L, -1, "Player");
	return scriptInterface->callFunction(1);
}

bool CreatureEvent::executeOnLogout(Player* player) const
{
	//onLogout(player)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeOnLogout] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);
	LuaScriptInterface::pushUserdata(L, player);
	LuaScriptInterface::setMetatable(L, -1, "Player");
	return scriptInterface->callFunction(1);
}

bool CreatureEvent::executeOnThink(Creature* creature, uint32_t interval)
{
	//onThink(creature, interval)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeOnThink] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);
	LuaScriptInterface::pushUserdata<Creature>(L, creature);
	LuaScriptInterface::setCreatureMetatable(L, -1, creature);
	lua_pushnumber(L, interval);

	return scriptInterface->callFunction(2);
}

bool CreatureEvent::executeOnPrepareDeath(Creature* creature, Creature* killer)
{
	//onPrepareDeath(creature, killer)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeOnPrepareDeath] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);

	LuaScriptInterface::pushUserdata<Creature>(L, creature);
	LuaScriptInterface::setCreatureMetatable(L, -1, creature);

	if (killer) {
		LuaScriptInterface::pushUserdata<Creature>(L, killer);
		LuaScriptInterface::setCreatureMetatable(L, -1, killer);
	} else {
		lua_pushnil(L);
	}

	return scriptInterface->callFunction(2);
}

bool CreatureEvent::executeOnDeath(Creature* creature, Item* corpse, Creature* killer, Creature* mostDamageKiller, bool lastHitUnjustified, bool mostDamageUnjustified)
{
	//onDeath(creature, corpse, lasthitkiller, mostdamagekiller, lasthitunjustified, mostdamageunjustified)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeOnDeath] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);
	LuaScriptInterface::pushUserdata<Creature>(L, creature);
	LuaScriptInterface::setCreatureMetatable(L, -1, creature);

	LuaScriptInterface::pushThing(L, corpse);

	if (killer) {
		LuaScriptInterface::pushUserdata<Creature>(L, killer);
		LuaScriptInterface::setCreatureMetatable(L, -1, killer);
	} else {
		lua_pushnil(L);
	}

	if (mostDamageKiller) {
		LuaScriptInterface::pushUserdata<Creature>(L, mostDamageKiller);
		LuaScriptInterface::setCreatureMetatable(L, -1, mostDamageKiller);
	} else {
		lua_pushnil(L);
	}

	LuaScriptInterface::pushBoolean(L, lastHitUnjustified);
	LuaScriptInterface::pushBoolean(L, mostDamageUnjustified);

	return scriptInterface->callFunction(6);
}

bool CreatureEvent::executeAdvance(Player* player, skills_t skill, uint32_t oldLevel,
                                       uint32_t newLevel)
{
	//onAdvance(player, skill, oldLevel, newLevel)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeAdvance] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);
	LuaScriptInterface::pushUserdata(L, player);
	LuaScriptInterface::setMetatable(L, -1, "Player");
	lua_pushnumber(L, static_cast<uint32_t>(skill));
	lua_pushnumber(L, oldLevel);
	lua_pushnumber(L, newLevel);

	return scriptInterface->callFunction(4);
}

void CreatureEvent::executeOnKill(Creature* creature, Creature* target, bool lastHit)
{
	//onKill(creature, target, lastHit)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeOnKill] Call stack overflow" << std::endl;
		return;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);
	LuaScriptInterface::pushUserdata<Creature>(L, creature);
	LuaScriptInterface::setCreatureMetatable(L, -1, creature);
	LuaScriptInterface::pushUserdata<Creature>(L, target);
	LuaScriptInterface::setCreatureMetatable(L, -1, target);
	LuaScriptInterface::pushBoolean(L, lastHit);
	scriptInterface->callVoidFunction(3);
}

void CreatureEvent::executeModalWindow(Player* player, uint32_t modalWindowId, uint8_t buttonId, uint8_t choiceId)
{
	//onModalWindow(player, modalWindowId, buttonId, choiceId)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeModalWindow] Call stack overflow" << std::endl;
		return;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();
	scriptInterface->pushFunction(scriptId);

	LuaScriptInterface::pushUserdata(L, player);
	LuaScriptInterface::setMetatable(L, -1, "Player");

	lua_pushnumber(L, modalWindowId);
	lua_pushnumber(L, buttonId);
	lua_pushnumber(L, choiceId);

	scriptInterface->callVoidFunction(4);
}

bool CreatureEvent::executeTextEdit(Player* player, Item* item, const std::string& text)
{
	//onTextEdit(player, item, text)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeTextEdit] Call stack overflow" << std::endl;
		return false;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();
	scriptInterface->pushFunction(scriptId);

	LuaScriptInterface::pushUserdata(L, player);
	LuaScriptInterface::setMetatable(L, -1, "Player");

	LuaScriptInterface::pushThing(L, item);
	LuaScriptInterface::pushString(L, text);

	return scriptInterface->callFunction(3);
}

void CreatureEvent::executeHealthChange(Creature* creature, Creature* attacker, CombatDamage& damage)
{
	//onHealthChange(creature, attacker, primaryDamage, primaryType, secondaryDamage, secondaryType, origin)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeHealthChange] Call stack overflow" << std::endl;
		return;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();
	scriptInterface->pushFunction(scriptId);

	LuaScriptInterface::pushUserdata(L, creature);
	LuaScriptInterface::setCreatureMetatable(L, -1, creature);
	if (attacker) {
		LuaScriptInterface::pushUserdata(L, attacker);
		LuaScriptInterface::setCreatureMetatable(L, -1, attacker);
	} else {
		lua_pushnil(L);
	}

	LuaScriptInterface::pushCombatDamage(L, damage);

	if (scriptInterface->protectedCall(L, 7, 4) != 0) {
		LuaScriptInterface::reportError(nullptr, LuaScriptInterface::popString(L));
	} else {
		damage.primary.value = std::abs(LuaScriptInterface::getNumber<int32_t>(L, -4));
		damage.primary.type = LuaScriptInterface::getNumber<CombatType_t>(L, -3);
		damage.secondary.value = std::abs(LuaScriptInterface::getNumber<int32_t>(L, -2));
		damage.secondary.type = LuaScriptInterface::getNumber<CombatType_t>(L, -1);

		lua_pop(L, 4);
		if (damage.primary.type != COMBAT_HEALING) {
			damage.primary.value = -damage.primary.value;
			damage.secondary.value = -damage.secondary.value;
		}
	}

	scriptInterface->resetScriptEnv();
}

void CreatureEvent::executeManaChange(Creature* creature, Creature* attacker, CombatDamage& damage)
{
	//onManaChange(creature, attacker, primaryDamage, primaryType, secondaryDamage, secondaryType, origin)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeManaChange] Call stack overflow" << std::endl;
		return;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();
	scriptInterface->pushFunction(scriptId);

	LuaScriptInterface::pushUserdata(L, creature);
	LuaScriptInterface::setCreatureMetatable(L, -1, creature);
	if (attacker) {
		LuaScriptInterface::pushUserdata(L, attacker);
		LuaScriptInterface::setCreatureMetatable(L, -1, attacker);
	} else {
		lua_pushnil(L);
	}

	LuaScriptInterface::pushCombatDamage(L, damage);

	if (scriptInterface->protectedCall(L, 7, 4) != 0) {
		LuaScriptInterface::reportError(nullptr, LuaScriptInterface::popString(L));
	} else {
		damage.primary.value = LuaScriptInterface::getNumber<int32_t>(L, -4);
		damage.primary.type = LuaScriptInterface::getNumber<CombatType_t>(L, -3);
		damage.secondary.value = LuaScriptInterface::getNumber<int32_t>(L, -2);
		damage.secondary.type = LuaScriptInterface::getNumber<CombatType_t>(L, -1);
		lua_pop(L, 4);
	}

	scriptInterface->resetScriptEnv();
}

void CreatureEvent::executeExtendedOpcode(Player* player, uint8_t opcode, const std::string& buffer)
{
	//onExtendedOpcode(player, opcode, buffer)
	if (!scriptInterface->reserveScriptEnv()) {
		std::cout << "[Error - CreatureEvent::executeExtendedOpcode] Call stack overflow" << std::endl;
		return;
	}

	ScriptEnvironment* env = scriptInterface->getScriptEnv();
	env->setScriptId(scriptId, scriptInterface);

	lua_State* L = scriptInterface->getLuaState();

	scriptInterface->pushFunction(scriptId);

	LuaScriptInterface::pushUserdata<Player>(L, player);
	LuaScriptInterface::setMetatable(L, -1, "Player");

	lua_pushnumber(L, opcode);
	LuaScriptInterface::pushString(L, buffer);

	scriptInterface->callVoidFunction(3);
}
