project('wayland-demo','c')

dir_prefix = get_option('prefix')
dir_bin = join_paths(dir_prefix, get_option('bindir'))
dir_lib = join_paths(dir_prefix, get_option('libdir'))
dir_lib_pc = join_paths(dir_lib, 'pkgconfig')
dir_data = join_paths(dir_prefix, get_option('datadir'))
public_inc = join_paths(dir_prefix, get_option('includedir'))
common_inc = [ include_directories('.'), include_directories('./common'), public_inc ]

pkgconfig = import('pkgconfig')
cc = meson.get_compiler('c')
dep_wayland_client = dependency('wayland-client', version: '>= 1.18.0')
dep_libm = cc.find_library('m')

subdir('protocol')

demo_srcs=[
    {
        'name':'shm',
        'sources': [
            'simple-shm.c',
			'shared/os-compatibility.c',
			'common/buffer.c',
			'common/display.c',
			'common/render.c',
			'common/window.c',
            xdg_shell_client_protocol_h,
			xdg_shell_protocol_c,
			fullscreen_shell_unstable_v1_client_protocol_h,
            fullscreen_shell_unstable_v1_protocol_c,
        ],
        'dep_objs':[ dep_wayland_client], 
		'deps': [ 'egl', 'wayland-egl', 'glesv2', 'wayland-cursor' ]
    },
	{
        'name':'egl',
        'sources': [
            'simple-egl.c',
			'shared/os-compatibility.c',
			'common/buffer.c',
			'common/display.c',
			'common/render.c',
			'common/window.c',
			'opengl/egl.c',
			'opengl/opengl.c',
            xdg_shell_client_protocol_h,
			xdg_shell_protocol_c,
			fullscreen_shell_unstable_v1_client_protocol_h,
            fullscreen_shell_unstable_v1_protocol_c,
        ],
        'dep_objs':[ dep_wayland_client, dep_libm ],
		'deps': [ 'egl', 'wayland-egl', 'glesv2', 'wayland-cursor' ]
    }
]

foreach t : demo_srcs
    t_name = 'simple-' + t.get('name')
	t_deps = t.get('dep_objs', [])	
		
	foreach depname : t.get('deps', [])
		dep = dependency(depname, required: false)
		if not dep.found()
			error('@0@ requires @1@ which was not found. If you rather not build this, drop "@2@" from simple-clients option.'.format(t_name, depname, t.get('name')))
		endif
		t_deps += dep
	endforeach

	foreach optname : t.get('options', [])
		if not get_option(optname)
				error('@0@ requires option @1@ which is not enabled. If you rather not build this, drop "@2@" from simple-clients option.'.format(t_name, optname, t.get('name')))
		endif
	endforeach

	executable(
		t_name, t.get('sources'),
		include_directories: common_inc,
		dependencies: t_deps,
		install: false
	)

endforeach
