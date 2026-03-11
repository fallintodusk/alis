// Copyright ALIS. All Rights Reserved.

#pragma once

/**
 * Standard GAS attribute accessor macros.
 * Include this header in AttributeSet classes instead of defining ATTRIBUTE_ACCESSORS locally.
 */

#ifndef ATTRIBUTE_ACCESSORS
#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
#endif
