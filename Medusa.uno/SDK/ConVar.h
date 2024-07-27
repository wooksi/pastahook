#pragma once

#include <type_traits>

#include "Platform.h"
#include "UtlVector.h"
#include "VirtualMethod.h"
#include "Pad.h"
#define FORCEINLINE_CVAR inline

struct ConVar {
    VIRTUAL_METHOD(float, getFloat, 12, (), (this))
    VIRTUAL_METHOD(int, getInt, 13, (), (this))
    VIRTUAL_METHOD(void, setValue, 14, (const char* value), (this, value))
    VIRTUAL_METHOD(void, setValue, 15, (float value), (this, value))
    VIRTUAL_METHOD(void, setValue, 16, (int value), (this, value))


    PAD(24)
    std::add_pointer_t<void __cdecl()> changeCallback;
    ConVar* parent;
    const char* defaultValue;
    char* string;
    PAD(44)
    UtlVector<void(__cdecl*)()> onChangeCallbacks;
    float GetFloat(void) const;

    struct CVValue_t
    {
        char* m_pszString;
        int     m_StringLength;
        float   m_fValue;
        int     m_nValue;
    };
    CVValue_t   m_Value;
};

FORCEINLINE_CVAR float ConVar::GetFloat(void) const
{
    uint32_t xored = *(uint32_t*)&parent->m_Value.m_fValue ^ (uint32_t)this;
    return *(float*)&xored;
}
