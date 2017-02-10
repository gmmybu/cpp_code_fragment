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

////////////////////////////////////////////////////////////////////////////////////////////////////

lru_cache.h

when used with pointer type, later call to 'query' may cause previous returned values by 'query' invalid, use it carefully.

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
