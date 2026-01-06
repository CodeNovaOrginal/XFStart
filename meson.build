project('xfstart', 'c', version: '0.1.0', default_options: ['warning_level=2', 'c_std=c11'])

glib = dependency('glib-2.0')
gio = dependency('gio-2.0')
gtk = dependency('gtk+-3.0')
panel = dependency('libxfce4panel-2.0')

srcs = [
  'src/plugin.c',
  'src/menu.c',
]

executable('xfstart', srcs,
  dependencies: [glib, gio, gtk, panel],
  install: true
)

install_data('data/xfstart.desktop',
  install_dir: join_paths(get_option('prefix'), 'share', 'xfce4', 'panel', 'plugins')
)
