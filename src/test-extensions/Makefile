all: logo.so tone.so

libafb_required_version = 5.0.0
$(shell pkg-config libafbcli --atleast-version $(libafb_required_version))
ifneq (0, $(.SHELLSTATUS))
  $(error ERROR required version of libafb is $(libafb_required_version) \
          but found version $(shell pkg-config libafbcli --modversion))
endif

flgs = $(shell pkg-config libafbcli --cflags --libs)

.SUFFIXES: .so

%.so: %.c
	gcc -shared -o $@ $< -fPIC $(flgs)

