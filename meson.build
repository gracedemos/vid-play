project('vid-play', 'c')

if host_machine.system() == 'darwin'
    inc = include_directories('/opt/homebrew/include')
else
    inc = include_directories()
endif

lavf = dependency('libavformat')
lavc = dependency('libavcodec')
lavu = dependency('libavutil')
sdl3 = dependency('sdl3')

executable(
    'vid-play',
    'src/main.c',
    include_directories: inc,
    dependencies: [lavf, lavc, lavu, sdl3],
)
