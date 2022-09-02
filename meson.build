project('obs-lv2', 'cpp', license : 'GPL2+')

fs = import('fs')

deps =  [
  dependency('lv2', version : '>=1.16.0'),
  dependency('libobs'),
  dependency('lilv-0'),
  dependency('suil-0'),
  dependency('Qt5Widgets'),
  dependency('threads'),
]

sources = [
  'obs-lv2.cpp',
  'ui.cpp',
  'ui_timer.cpp',
  'urid.cpp',
  'state.cpp',
  'ports.cpp',
  'worker.cpp',
  'core.cpp',
]

if get_option('local_install')
  if host_machine.cpu_family() != 'x86_64'
    error('local_install is supported only on x86_64 systems for now')
  endif
  if host_machine.system() != 'linux'
    error('local_install is supported only on Linux systems for now')
  endif

  so_install_dir = fs.expanduser('~/.config/obs-studio/plugins/obs-lv2/bin/64bit/')
else
  so_install_dir = get_option('libdir') / 'obs-plugins'
endif

shared_library(meson.project_name(),
	       sources,
	       dependencies : deps,
	       name_prefix : '',
	       install: true,
	       install_dir : so_install_dir)
