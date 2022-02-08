if get_option('fish-completions')
	fish_files = files('fish/grim.fish')

	fish_comp = dependency('fish', required: false)
	if fish_comp.found()
		fish_install_dir = fish_comp.get_variable('completionsdir')
	else
		datadir = get_option('datadir')
		fish_install_dir = join_paths(datadir, 'fish', 'vendor_completions.d')
	endif
	install_data(fish_files, install_dir: fish_install_dir)
endif


if get_option('bash-completions')
	bash_comp = dependency('bash-completion')
	bash_files = files('bash/grim.bash')
	bash_install_dir = bash_comp.get_variable('completionsdir')
	install_data(bash_files, install_dir: bash_install_dir)
endif
