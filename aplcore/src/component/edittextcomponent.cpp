/**
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *     http://aws.amazon.com/apache2.0/
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
#include <string>

#include "apl/component/componentpropdef.h"
#include "apl/component/edittextcomponent.h"
#include "apl/component/yogaproperties.h"
#include "apl/content/rootconfig.h"
#include "apl/engine/event.h"
#include "apl/focus/focusmanager.h"
#include "apl/primitives/characterrange.h"
#include "apl/time/sequencer.h"
#include "apl/touch/pointerevent.h"

namespace apl {

CoreComponentPtr
EditTextComponent::create(const ContextPtr& context,
                          Properties&& properties,
                          const Path& path)
{
    auto ptr = std::make_shared<EditTextComponent>(context, std::move(properties), path);
    ptr->initialize();
    return ptr;
}

EditTextComponent::EditTextComponent(const ContextPtr& context,
                                     Properties&& properties,
                                     const Path& path)
        : ActionableComponent(context, std::move(properties), path)
{
    YGNodeSetMeasureFunc(mYGNodeRef, textMeasureFunc);
    YGNodeSetBaselineFunc(mYGNodeRef, textBaselineFunc);
    YGNodeSetNodeType(mYGNodeRef, YGNodeTypeText);
}

/*
 * Initial assignment of properties.  Don't set any dirty flags here; this
 * all should be running in the constructor.
 *
 * This method initializes the values of the border corners.
*/
void
EditTextComponent::assignProperties(const ComponentPropDefSet& propDefSet)
{
    ActionableComponent::assignProperties(propDefSet);
    calculateDrawnBorder(false);
    parseValidCharactersProperty();

    // Calculate initial measurement hash.
    fixTextMeasurementHash();
}

void
EditTextComponent::preLayoutProcessing(bool useDirtyFlag)
{
    ActionableComponent::preLayoutProcessing(useDirtyFlag);

    // Update text measurement hash as some properties may have changed it
    // and we actually need it on layout time
    fixTextMeasurementHash();
}

static inline Object defaultFontColor(Component& component, const RootConfig& rootConfig)
{
    return Object(rootConfig.getDefaultFontColor(component.getContext()->getTheme()));
}

static inline Object defaultFontFamily(Component& component, const RootConfig& rootConfig)
{
    return Object(rootConfig.getDefaultFontFamily());
}

static inline Object inheritLang(Component& comp, const RootConfig& rconfig)
{
    return Object(comp.getContext()->getLang());
};

static inline Object defaultHighlightColor(Component& component, const RootConfig& rootConfig)
{
    return Object(rootConfig.getDefaultHighlightColor(component.getContext()->getTheme()));
}

const ComponentPropDefSet&
EditTextComponent::propDefSet() const
{
    static ComponentPropDefSet sEditTextComponentProperties(ActionableComponent::propDefSet(), {
            {kPropertyBorderColor,              Color(),                        asColor,                        kPropInOut | kPropStyled | kPropDynamic | kPropVisualHash},
            {kPropertyBorderWidth,              Dimension(0),                   asNonNegativeAbsoluteDimension, kPropInOut | kPropStyled | kPropDynamic,                                yn::setBorder<YGEdgeAll>},
            {kPropertyColor,                    Color(),                        asColor,                        kPropInOut | kPropStyled | kPropDynamic | kPropVisualHash,                                defaultFontColor},
            {kPropertyFontFamily,               "",                             asString,                       kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash,  defaultFontFamily},
            {kPropertyFontSize,                 Dimension(40),                  asAbsoluteDimension,            kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash},
            {kPropertyFontStyle,                kFontStyleNormal,               sFontStyleMap,                  kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash},
            {kPropertyFontWeight,               400,                            sFontWeightMap,                 kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash},
            {kPropertyHighlightColor,           Color(),                        asColor,                        kPropInOut | kPropStyled | kPropDynamic | kPropVisualHash,                                defaultHighlightColor},
            {kPropertyHint,                     "",                             asString,                       kPropInOut | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash},
            {kPropertyHintColor,                Color(),                        asColor,                        kPropInOut | kPropStyled | kPropDynamic | kPropVisualHash,                                defaultFontColor},
            {kPropertyHintStyle,                kFontStyleNormal,               sFontStyleMap,                  kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash},
            {kPropertyHintWeight,               400,                            sFontWeightMap,                 kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash},
            {kPropertyKeyboardType,             kKeyboardTypeNormal,            sKeyboardTypeMap,               kPropInOut | kPropStyled},
            {kPropertyLang,                     "",                             asString,                       kPropInOut | kPropLayout | kPropStyled | kPropDynamic | kPropTextHash | kPropVisualHash,  inheritLang},
            {kPropertyMaxLength,                0,                              asInteger,                      kPropInOut | kPropStyled | kPropTextHash | kPropVisualHash},
            {kPropertyOnSubmit,                 Object::EMPTY_ARRAY(),          asCommand,                      kPropIn},
            {kPropertyOnTextChange,             Object::EMPTY_ARRAY(),          asCommand,                      kPropIn},
            {kPropertySecureInput,              false,                          asBoolean,                      kPropInOut | kPropStyled | kPropDynamic},
            {kPropertyKeyboardBehaviorOnFocus,  kBehaviorOnFocusSystemDefault,  sKeyboardBehaviorOnFocusMap,    kPropIn | kPropStyled},
            {kPropertySelectOnFocus,            false,                          asBoolean,                      kPropInOut | kPropStyled},
            {kPropertySize,                     8,                              asPositiveInteger,              kPropInOut | kPropStyled | kPropLayout},
            {kPropertySubmitKeyType,            kSubmitKeyTypeDone,             sSubmitKeyTypeMap,              kPropInOut | kPropStyled},
            {kPropertyText,                     "",                             asString,                       kPropInOut | kPropDynamic | kPropVisualContext | kPropTextHash | kPropVisualHash},
            {kPropertyValidCharacters,          "",                             asString,                       kPropIn | kPropStyled},

            // The width of the drawn border.  If borderStrokeWith is set, the drawn border is the min of borderWidth
            // and borderStrokeWidth.  If borderStrokeWidth is unset, the drawn border defaults to borderWidth
            {kPropertyBorderStrokeWidth,        Object::NULL_OBJECT(),          asNonNegativeAbsoluteDimension, kPropIn | kPropStyled | kPropDynamic,                                                     resolveDrawnBorder},
            {kPropertyDrawnBorderWidth,         Object::NULL_OBJECT(),          asNonNegativeAbsoluteDimension, kPropOut | kPropVisualHash},
    });

    return sEditTextComponentProperties;
}

const EventPropertyMap&
EditTextComponent::eventPropertyMap() const
{
    static EventPropertyMap sEditTextEventProperties = eventPropertyMerge(
        CoreComponent::eventPropertyMap(),
        {
            {"text",  [](const CoreComponent *c) { return c->getCalculated(kPropertyText); }},
            {"color", [](const CoreComponent *c) { return c->getCalculated(kPropertyColor); }},
        });

    return sEditTextEventProperties;
}

Object
EditTextComponent::getValue() const
{
    return mCalculated.get(kPropertyText);
}

void
EditTextComponent::update(UpdateType type, float value)
{
    if (type == kUpdateSubmit) {
        ContextPtr eventContext = createEventContext("Submit");
        auto commands = getCalculated(kPropertyOnSubmit);
        mContext->sequencer().executeCommands(commands, eventContext, shared_from_corecomponent(), false);

    } else
    CoreComponent::update(type, value);
}

void
EditTextComponent::update(UpdateType type, const std::string& value)
{
    if (type == kUpdateTextChange) {
        auto requestedValue = Object(value);
        auto currentValue = mCalculated.get(kPropertyText);
        if (requestedValue != currentValue) {
            if (getRootConfig().experimentalFeatureEnabled(RootConfig::kExperimentalFeatureMarkEditTextDirtyOnUpdate)) {
                setProperty(kPropertyText, value);
            } else {
                mCalculated.set(kPropertyText, value);
            }
            ContextPtr eventContext = createEventContext("TextChange");
            auto commands = getCalculated(kPropertyOnTextChange);
            mContext->sequencer().executeCommands(commands, eventContext, shared_from_corecomponent(), false);
        }
    } else
        CoreComponent::update(type, value);
}

bool
EditTextComponent::isCharacterValid(const wchar_t wc) const
{
    if (mCharacterRangesPtr == nullptr) return true;

    std::vector<CharacterRangeData> validRanges = mCharacterRangesPtr->getRanges();
    if (validRanges.empty()) return true;

    for (auto& range : validRanges) {
        if (range.isCharacterValid(wc)) return true;
    }
    return false;
}

void EditTextComponent::parseValidCharactersProperty()
{
    mCharacterRangesPtr = std::make_shared<CharacterRanges>(CharacterRanges(getContext()->session(),
            mCalculated.get(kPropertyValidCharacters).asString()));
}

PointerCaptureStatus
EditTextComponent::processPointerEvent(const PointerEvent& event, apl_time_t timestamp)
{
    auto pointerStatus = ActionableComponent::processPointerEvent(event, timestamp);
    if (pointerStatus != kPointerStatusNotCaptured)
        return pointerStatus;

    if (getRootConfig().experimentalFeatureEnabled(RootConfig::kExperimentalFeatureFocusEditTextOnTap) &&
            event.pointerEventType == kPointerUp) {
        getContext()->focusManager().setFocus(shared_from_corecomponent(), true);
        return kPointerStatusPendingCapture;
    }
    if (getRootConfig().experimentalFeatureEnabled(RootConfig::kExperimentalFeatureRequestKeyboard) &&
            event.pointerEventType == kPointerUp) {
        getContext()->pushEvent(Event(kEventTypeOpenKeyboard, shared_from_this()));
        return kPointerStatusPendingCapture;
    }

    return kPointerStatusNotCaptured;
}

void
EditTextComponent::executeOnFocus() {
    ActionableComponent::executeOnFocus();

    if (getCalculated(kPropertyKeyboardBehaviorOnFocus) == kBehaviorOnFocusOpenKeyboard) {
        getContext()->pushEvent(Event(kEventTypeOpenKeyboard, shared_from_this()));
    }
}

}  // namespace apl