project(
  'qtphy', 'cpp',
  version : '0.3.0',
  license : 'MIT',
  meson_version : '>=0.63.0'
)

qt6 = import('qt6')
qt6_dep = dependency('qt6', modules : ['Core', 'Qml', 'Quick', 'Gui', 'DBus', 'Multimedia'])
gstreamer_dep = dependency('gstreamer-1.0')

headers = [
  'src/device_info.hpp',
  'src/rauc.hpp'
  'src/camera_demo.hpp'
]
src = [
  'src/main.cpp',
  'src/device_info.cpp',
  'src/rauc.cpp'
  'src/camera_demo.cpp'
]

inc = include_directories('src')
moc = qt6.compile_moc(
  headers : headers,
  include_directories : inc,
  dependencies : qt6_dep
)
res = qt6.compile_resources(
  sources : 'resources/resources.qrc'
)


qt_preprocessed = qt6_mod.preprocess(qresources : 'qmlsink.qrc')
executable(
  meson.project_name(),
  'qml6sink',
  qt_preprocessed,
  sources : [src, res, moc],
  include_directories : [inc, configinc],
  dependencies : [qt6_dep, gstreamer_dep]
  c_args : gst_plugins_good_args,
  install : true
)
