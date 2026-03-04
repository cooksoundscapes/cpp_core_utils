project('audio-core', 'cpp',
  version : '0.1',
  default_options : ['cpp_std=c++17'])

# Dependência do JACK (o Meson busca via pkg-config automaticamente)
jack_dep = dependency('jack')

# Onde os headers estão
core_inc = include_directories('include')

# Arquivos fonte
core_sources = files(
  'src/jack.cpp',
  'src/logger.cpp'
)

# Criamos a biblioteca estática
core_lib = static_library('audio-core',
  core_sources,
  include_directories : core_inc,
  dependencies : jack_dep,
  install : true
)

# A parte mágica: O "declare_dependency" permite que o projeto principal
# receba os fontes, os headers e o link para o JACK de uma vez só.
core_dep = declare_dependency(
  include_directories : core_inc,
  link_with : core_lib,
  dependencies : jack_dep
)