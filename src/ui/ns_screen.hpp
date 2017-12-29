
#pragma once

#include "ui.h"
#include "items.hpp"

using namespace std::literals;

class PlaySoundSink : public IEventSink {
private:
    TRef<ISoundEngine> m_pSoundEngine;
    TRef<ISoundTemplate> m_pTemplate;

public:

    PlaySoundSink(ISoundEngine* pSoundEngine, ISoundTemplate* pTemplate) :
        m_pSoundEngine(pSoundEngine),
        m_pTemplate(pTemplate)
    {}

    bool OnEvent(IEventSource* pevent) {

        TRef<ISoundInstance> pSoundInstance;

        m_pTemplate->CreateSound(pSoundInstance, m_pSoundEngine->GetBufferSource(), NULL);

        return true;
    }
};

template <typename OriginalType, typename ResultType>
class EventMapper : public TEvent<ResultType>::SourceImpl, public TEvent<OriginalType>::Sink {
    TRef<typename TEvent<OriginalType>::Source> m_pOriginal;
    std::function<ResultType(const OriginalType&)> m_callback;

    TRef<typename TEvent<OriginalType>::Sink> m_pSinkDelegate;

public:
    EventMapper(const TRef<typename TEvent<OriginalType>::Source>& pOriginal, const std::function<ResultType(const OriginalType&)>& callback) : 
        m_pOriginal(pOriginal),
        m_callback(callback)
    {
        m_pSinkDelegate = TEvent<OriginalType>::Sink::CreateDelegate(this);
        m_pOriginal->AddSink(m_pSinkDelegate);
    }

    ~EventMapper() {
        if (m_pSinkDelegate) {
            m_pOriginal->RemoveSink(m_pSinkDelegate);
            m_pSinkDelegate = nullptr;
        }
    }

    bool OnEvent(typename TEvent<OriginalType>::Source* pevent, const OriginalType& value) override {
        Trigger(m_callback(value));
        return true;
    }

};

class ScreenNamespace {
public:
    static void AddNamespace(LuaScriptContext& context) {
        sol::table table = context.GetLua().create_table();

        table["Get"] = [&context](sol::this_state s, std::string name) {
            return context.GetScreenGlobals().GetExposer(name)->ExposeSolObject(s);
        };
        table["GetString"] = [&context](std::string name) {
            return context.GetScreenGlobals().Get<TRef<StringValue>>(name);
        };
        table["GetNumber"] = [&context](std::string name) {
            return context.GetScreenGlobals().Get<TRef<Number>>(name);
        };
        table["GetBool"] = [&context](std::string name) {
            return context.GetScreenGlobals().Get<TRef<Boolean>>(name);
        };
        table["GetState"] = [&context](std::string name) {
            return context.GetScreenGlobals().Get<TRef<UiStateValue>>(name);
        };

        context.GetLua().new_usertype<TRef<UiObjectContainer>>("UiObjectContainer",
            "new", sol::no_constructor,
            "Get", [](sol::this_state s, TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->GetExposer(key)->ExposeSolObject(s);
            },
            "GetString", [](TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->GetString(key);
            },
            "GetNumber", [](TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->GetNumber(key);
            },
            "GetBool", [](TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->GetBoolean(key);
            },
            "GetState", [](TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->Get<TRef<UiStateValue>>(key);
            },
            "GetEventSink", [](TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->Get<TRef<IEventSink>>(key);
            },
            "GetList", [](TRef<UiObjectContainer> pointer, std::string key) {
                return pointer->Get<TRef<ContainerList>>(key);
            }
        );

        context.GetLua().new_usertype<UiState>("UiState", 
            "new", sol::no_constructor,
            "Get", [](sol::this_state s, const UiState& state, std::string key) {
                return state.GetExposer(key)->ExposeSolObject(s);
            },
            "GetString", [](const UiState& state, std::string key) {
                return state.GetString(key);
            },
            "GetNumber", [](const UiState& state, std::string key) {
                return state.GetNumber(key);
            },
            "GetBool", [](const UiState& state, std::string key) {
                return state.GetBoolean(key);
            },
            "GetState", [](const UiState& state, std::string key) {
                return state.Get<TRef<UiStateValue>>(key);
            },
            "GetEventSink", [](const UiState& state, std::string key) {
                return state.Get<TRef<IEventSink>>(key);
            },
            "GetList", [](const UiState& state, std::string key) {
                return state.Get<TRef<ContainerList>>(key);
            }
        );

        table["GetExternalEventSink"] = [&context](std::string path) {
            IEventSink& sink = context.GetExternalEventSink(path);
            return (TRef<IEventSink>)&sink;
        };

        table["CreatePlaySoundSink"] = [&context](std::string path) {
            TRef<ISoundTemplate> pTemplate;

            std::string full_path = context.FindPath(path);

            ZSucceeded(CreateWaveFileSoundTemplate(pTemplate, full_path.c_str()));
            return (TRef<IEventSink>)new PlaySoundSink(context.GetSoundEngine(), pTemplate);
        };

        table["CreateOpenWebsiteSink"] = [&context](std::string strWebsite) {
            auto openWebsite = context.GetOpenWebsiteFunction();

            return (TRef<IEventSink>)new CallbackSink([openWebsite, strWebsite]() {
                openWebsite(strWebsite);
                return true;
            });
        };
        
        table["GetResolution"] = [&context]() {
            return (TRef<PointValue>)new TransformedValue<Point, WinPoint>([](WinPoint winpoint) {
                return Point((float)winpoint.X(), (float)winpoint.Y());
            }, context.GetEngine()->GetResolutionSizeModifiable());
        };

        table["HasKeyboardFocus"] = [&context]() {
            return (TRef<SimpleModifiableValue<bool>>)context.HasKeyboardFocus();
        };

        table["GetKeyboardKeySource"] = [&context]() {
            return (TRef<TEvent<ZString>::Source>)new EventMapper<const KeyState&, ZString>(context.GetKeyboardSource(), [](const KeyState& ks) {
                if (ks.vk == 13) {
                    return ZString("");
                }
                else if (ks.vk == 8) {
                    //?
                    return ZString("");
                }
                else {
                    char ch = ks.vk;
                    ZString str = ZString(ch, 1);
                    return str;
                }

            });
        };

        context.GetLua().set("Screen", table);
    }
};
