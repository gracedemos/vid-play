project('vid-play', 'c')

inc = include_directories('/opt/homebrew/include')

lavf = dependency('libavformat')
sdl3 = dependency('sdl3')

executable('vid-play', 'src/main.c',
           include_directories : inc,
           dependencies : [lavf, sdl3])
