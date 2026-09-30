CC ?= cc
CFLAGS ?= -std=c11 -O2 -ffp-contract=off -Wall -Wextra -Iinclude -fPIC
LDFLAGS ?= -lm

SRC = \
  src/medium.c \
  src/trackers.c \
  src/state_recorder.c \
  src/env_flags.c \
  src/control.c

# Optional modules as they land
ifeq ($(wildcard src/mad.c),src/mad.c)
  SRC += src/mad.c
endif
ifeq ($(wildcard src/car_define.c),src/car_define.c)
  SRC += src/car_define.c
endif
ifeq ($(wildcard src/conto.c),src/conto.c)
  SRC += src/conto.c
endif
ifeq ($(wildcard src/stage_load.c),src/stage_load.c)
  SRC += src/stage_load.c
endif
ifeq ($(wildcard src/checkpoints.c),src/checkpoints.c)
  SRC += src/checkpoints.c
endif
ifeq ($(wildcard src/sort_cars.c),src/sort_cars.c)
  SRC += src/sort_cars.c
endif
ifeq ($(wildcard src/sim.c),src/sim.c)
  SRC += src/sim.c
endif

OBJ = $(SRC:src/%.c=build/%.o)
DIFF = build/nfmst_diff
BIN = build/nfm_sim
OCEAN = build/libnfm_ocean.a
GYM_LIB = build/libnfm_gym.dylib
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Linux)
  GYM_LIB = build/libnfm_gym.so
endif

REPO_ROOT := $(abspath ..)
TEST_STAGE ?= 11
TEST_CAR ?= 0
TEST_SEED ?= 1
TEST_DIR := build/parity

.PHONY: all clean test models ocean gym

all: models $(BIN) $(DIFF) ocean gym

models:
	@mkdir -p assets/models
	@test -f assets/models/2000tornados.rad || \
	  unzip -qo ../OBJ/data/models.zip -d assets/models

build:
	mkdir -p build

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN): tools/nfm_sim.c $(OBJ) | build
	$(CC) $(CFLAGS) tools/nfm_sim.c $(OBJ) -o $@ $(LDFLAGS) -pthread

$(DIFF): tools/nfmst_diff.c build/state_recorder.o | build
	$(CC) $(CFLAGS) tools/nfmst_diff.c build/state_recorder.o -o $@ $(LDFLAGS)

build/gym_capi.o: src/gym_capi.c include/nfm/gym_capi.h include/nfm/sim.h | build
	$(CC) $(CFLAGS) -fPIC -c src/gym_capi.c -o $@

gym: models build/gym_capi.o $(OBJ) | build
	$(CC) $(CFLAGS) -shared build/gym_capi.o $(OBJ) -o $(GYM_LIB) $(LDFLAGS)

# Ocean env object (links against sim once available)
build/nfm_ocean.o: ocean/nfm/nfm.c ocean/nfm/nfm.h | build
	$(CC) $(CFLAGS) -Iocean/nfm -c ocean/nfm/nfm.c -o $@

$(OCEAN): build/nfm_ocean.o $(OBJ) | build
	ar rcs $@ build/nfm_ocean.o $(OBJ)

ocean: build/nfm_ocean.o
	@echo "[ocean] Puffer-facing Env in ocean/nfm/ (c_reset/c_step/c_close + Log)"

clean:
	rm -rf build

# Full bit-identical gate: C nfm_sim vs Java --norender.
test: models $(BIN) $(DIFF)
	@mkdir -p $(TEST_DIR)
	@echo "[test] C dump stage=$(TEST_STAGE) car=$(TEST_CAR) seed=$(TEST_SEED)"
	./$(BIN) --root $(REPO_ROOT) --assets assets \
	  --stage $(TEST_STAGE) --car $(TEST_CAR) --seed $(TEST_SEED) --forward \
	  --out $(TEST_DIR)/c.nfmst
	cd $(REPO_ROOT) && ./record.sh $(TEST_STAGE) $(TEST_CAR) \
	  c/$(TEST_DIR)/java.nfmst \
	  --norender --java --seed $(TEST_SEED) --forward --timeout 0
	./$(DIFF) $(TEST_DIR)/java.nfmst $(TEST_DIR)/c.nfmst
	cmp $(TEST_DIR)/java.nfmst $(TEST_DIR)/c.nfmst
	@echo "BIT_IDENTICAL"
