subdir('libresrc')

aegisub_src = files(
    'MatroskaParser.c',
    'aegisublocale.cpp',
    'ass_attachment.cpp',
    'ass_dialogue.cpp',
    'ass_entry.cpp',
    'ass_file.cpp',
    'ass_karaoke.cpp',
    'ass_override.cpp',
    'ass_parser.cpp',
    'ass_style.cpp',
    'async_video_provider.cpp',
    'auto4_base.cpp',
    'auto4_lua.cpp',
    'auto4_lua_assfile.cpp',
    'auto4_lua_dialog.cpp',
    'auto4_lua_progresssink.cpp',
    'charset_detect.cpp',
    'colorspace.cpp',
    'command/command.cpp',
    'context.cpp',
    'dialog_progress.cpp',
    'export_fixstyle.cpp',
    'initial_line_state.cpp',
    'main.cpp',
    'project.cpp',
    'resolution_resampler.cpp',
    'selection_controller.cpp',
    'string_codec.cpp',
    'subs_controller.cpp',
    'subtitle_format.cpp',
    'subtitle_format_ass.cpp',
    'text_file_reader.cpp',
    'text_file_writer.cpp',
    'utils.cpp',
    'version.cpp',
    'video_controller.cpp',
    'video_frame.cpp',
    'video_provider_cache.cpp',
    'video_provider_dummy.cpp',
    'video_provider_manager.cpp',
    'video_provider_yuv4mpeg.cpp',
)

opt_src = [
    ['FFMS2', ['video_provider_ffmpegsource.cpp',
               'ffmpegsource_common.cpp']],
]

foreach opt: opt_src
    if dep_avail.contains(opt[0])
        aegisub_src += opt[1]
    endif
endforeach

aegisub_cpp_pch = ['include/agi_pre.h']
if cxx.get_id() == 'msvc'
    aegisub_cpp_pch += 'include/agi_pre.cpp'
endif

aegisub = executable('aegisub', aegisub_src, version_h, acconf,
                     link_with: [libresrc, libluabins, libaegisub],
                     include_directories: [libaegisub_inc, libresrc_inc, version_inc, deps_inc],
                     cpp_pch: aegisub_cpp_pch,
                     install: true,
                     install_dir: bindir,
                     dependencies: deps)
