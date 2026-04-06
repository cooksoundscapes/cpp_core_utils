project('audio-core', 'cpp',
  version : '0.1',
  default_options : ['cpp_std=c++20'])

all_deps = [dependency('jack')]

core_inc = include_directories('include')

# Arquivos fonte
core_sources = files(
  'src/jack.cpp',
  'src/logger.cpp'
)

core_lib = static_library('audio-core',
  core_sources,
  include_directories : core_inc,
  dependencies : jack_dep,
  install : true
)

# Test setup
if not meson.is_subproject()
  catch2_dep = dependency('catch2')

  test_logger = executable(
    'test_logger',
    'tests/test_logger.cpp',
    include_directories: core_inc,
    dependencies: catch2_dep,   
    link_with: core_lib,
    cpp_args: ['-pthread']
  )

  test('logger', test_logger)
endif

if get_option('use_kfr')
  subdir('meson')
  deps += kfr_all_dep
endif

core_dep = declare_dependency(
  include_directories : core_inc,
  link_with : core_lib,
  dependencies : jack_dep
)