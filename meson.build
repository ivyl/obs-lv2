project('obs-lv2', 'cpp', license : 'GPL2+')

deps =  [
  dependency('lv2'),
  dependency('libobs'),
  dependency('lilv-0'),
  dependency('suil-0'),
  dependency('Qt5Widgets'),
]

shared_library(meson.project_name(),
	       'main.cpp',
	       dependencies : deps,
	       name_prefix : '',
	       install_dir : 'obs-plugins')
