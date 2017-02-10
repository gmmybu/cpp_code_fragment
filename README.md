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
