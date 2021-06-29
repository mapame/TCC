project('tcc', 'c',
  default_options : ['warning_level=1', 'buildtype=debugoptimized'])

cc = meson.get_compiler('c')

common_sources = ['src/common/communication.c', 'src/common/logger.c']

common_deps = [dependency('libbsd'), dependency('libcrypto'), cc.find_library('m', required : false)]

remotecontrol_sources = ['src/remote-control/main.c', 'src/remote-control/tftp.c']

backend_sources =	['src/backend/main.c',
					'src/backend/http.c',
					'src/backend/data_acquisition.c',
					'src/backend/disaggregation.c',
					'src/backend/meter_events.c',
					'src/backend/http_meter.c',
					'src/backend/config.c',
					'src/backend/http_config.c',
					'src/backend/http_dashboard.c',
					'src/backend/power.c',
					'src/backend/http_power.c',
					'src/backend/auth.c',
					'src/backend/http_auth.c',
					'src/backend/http_appliances.c',
					'src/backend/users.c',
					'src/backend/http_users.c']


executable('tcc-remote-control',
			sources: [common_sources, remotecontrol_sources],
			include_directories: 'src/common',
			dependencies: [common_deps])

executable('tcc-data-export',
			sources: [common_sources, 'src/data-export/main.c'],
			include_directories: 'src/common',
			dependencies: [common_deps])

executable('tcc-backend',
			include_directories: 'src/common',
			sources : [common_sources, backend_sources],
			dependencies: [common_deps, dependency('threads'), dependency('libmicrohttpd'), dependency('json-c'), dependency('uuid'), dependency('sqlite3'), cc.find_library('svm')])
