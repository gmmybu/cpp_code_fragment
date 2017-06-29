# cpp_code_fragment

event_stream.h

simulate C# event, to simplify event notify. you can link multiple event_streams to implement reactive style programing.

sample:

some place defines:

    event_stream<void(uint32_t)> on_player_end;
   
some place defines:

    event_stream<void()> on_video_play_end;

    event_stream<void()> on_audio_play_end;
    
forward on_player_end event:
    
    theApp.on_player_end.subscribe([=](uint32_t source_id) {
        if (source_id == _video_source) {
            on_video_play_end.emit();
        } else if (source_id == _audio_source) {
            on_audio_play_end.emit();
        } 
    });
    
warning : you should not delete any paramter either directly or indirectly in any callback, because later callbacks may use them. a simple solution is just pass by value.

here is the sample:
    
    std::vector<std::string> _texts;
    
    event_stream<void(const std::string&)> event_with_text;
    
    /// first callback
    event_with_text.subscribe([](const std::string& text) {
        for (auto iter = _texts.begin(); iter != _text.end(); iter++) {
            if (*iter == text) {
                _texts.erase(iter);
                break;
            }
        }
    });
    
    /// second callback
    event_with_text.subscribe([](const std::string& text) {
        printf("%s\n", text.c_str());
    });
   
    /// when called like this, no problem
    event_with_text.emit("hello");
    
    /// when called like this, big problem
    auto iter = _texts.begin();
    event_with_text.emit(*iter);
    
////////////////////////////////////////////////////////////////////////////////////////////////////

lru_cache.h

the algorithm is traditional, it uses a double_linked_list to record using order and a map to fast locate double_linked_list node. 

when used with pointer type, later call to 'query' may cause previous returned pointer by 'query' invalid, use it carefully.

but you can add life-cycle management whith an extra 'release' semantics to get more detailed control.

lru_cache needs a creator and a deletor (can use default deletor), the creator respond to create a value with a key and the deletor respond to delete a value.

creator sample:

    template<class K, class V>
    bool sample_creator_function(const K& k, V& v);

    template<class K, class V>
    struct sample_creator_functor
    {
        bool operator()(const K& k, V& v);
    };

deletor sample:

    template<class V>
    bool sample_deletor_function(const V& v);

    template<class V>
    struct sample_deletor_functor
    {
        bool operator()(const V& v);
    };
    
you can define it not same with these but should be compatible with these.

////////////////////////////////////////////////////////////////////////////////////////////////////

delayed_runner.h

it's not a brand new idea, you may see it several times. it uses auto executed class destructor to run some code when it's going out of its scope. here's an example:

    HANDLE enum_handle = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (enum_handle == INVALID_HANDLE_VALUE)
        return;
        
    will_delayed_run_for_sure([=] { CloseHandle(enum_handle); });
    
    /// other codes that use enum_handle, and you will not have to remember to release enum_handle

