_grim() {
	_init_completion || return

	local CUR PREV
	CUR="${COMP_WORDS[COMP_CWORD]}"
	PREV="${COMP_WORDS[COMP_CWORD-1]}"

	if [[ "$PREV" == "-t" ]]; then
		COMPREPLY=($(compgen -W "png ppm jpeg" -- "$CUR"))
		return
	elif [[ "$PREV" == "-o" ]]; then
		local OUTPUTS
		OUTPUTS="$(swaymsg -t get_outputs 2>/dev/null | \
			jq -r '.[] | select(.active) | "\(.name)\t\(.make) \(.model)"' 2>/dev/null)"

		COMPREPLY=($(compgen -W "$OUTPUTS" -- "$CUR"))
		return
	fi

	if [[ "$CUR" == -* ]]; then
		COMPREPLY=($(compgen -W "-h -s -g -t -q -o -c" -- "$CUR"))
		return
	fi

	# fall back to completing filenames
	_filedir
}

complete -F _grim grim
