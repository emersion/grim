function complete_outputs
    if string length -q "$SWAYSOCK"; and command -sq jq
        swaymsg -t get_outputs | jq -r '.[] | select(.active) | "\(.name)\t\(.make) \(.model)"'
    else
        return 1
    end
end

complete -c grim -s t --exclusive --arguments 'png ppm jpeg' -d 'Output image format'
complete -c grim -s q --exclusive -d 'Output jpeg quality (default 80)'
complete -c grim -s g --exclusive -d 'Region to capture: <x>,<y> <w>x<h>'
complete -c grim -s s --exclusive -d 'Output image scale factor'
complete -c grim -s c -d 'Include cursors in the screenshot'
complete -c grim -s h -d 'Show help and exit'
