all: logo.so tone.so

libafb_required_version = 5.0.0
$(shell pkg-config libafbcli --atleast-version $(libafb_required_version))
ifneq (0, $(.SHELLSTATUS))
  $(error ERROR required version of libafb is $(libafb_required_version) \
          but found version $(shell pkg-config libafbcli --modversion))
endif

flgs = $(shell pkg-config libafbcli --cflags --libs)

CFLAGS += -Wall
CFLAGS += -Werror=format-security
CFLAGS += -Wextra
CFLAGS += -Wconversion
CFLAGS += -Wincompatible-pointer-types
CFLAGS += -Wno-unused-parameter
CFLAGS += -Wsign-compare
CFLAGS += -Wsign-conversion
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -fanalyzer
CFLAGS += -fPIC
CFLAGS += -pipe
CFLAGS += -Werror=maybe-uninitialized
CFLAGS += -Werror=implicit-function-declaration

.SUFFIXES: .so

%.so: %.c
	$(CC) -shared -o $@ $< $(CFLAGS) $(flgs)

