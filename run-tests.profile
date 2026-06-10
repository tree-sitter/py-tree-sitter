include(default)

[settings]
build_type=RelWithDebInfo

{% if platform.system() != "Windows" %}
[conf]
tools.build:cflags=["-Wextra", "-fno-omit-frame-pointer"]
{% endif %}
