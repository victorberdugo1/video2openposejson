NAME = raypup
TEST_NAME = test_viewer

INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
TEST_OBJ_DIR = obj_test

SRC = main.c
TEST_SRC = test.c
WEB_SRC = web.c

SRCS = $(addprefix $(SRC_DIR)/, $(SRC))
TEST_SRCS = $(addprefix $(SRC_DIR)/, $(TEST_SRC))

OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TEST_OBJ = $(patsubst $(SRC_DIR)/%.c, $(TEST_OBJ_DIR)/%.o, $(TEST_SRCS))

RAYLIB_DIR = raylib
RAYLIB_LIB = libraylib.a
WEB_RAYLIB_LIB = libraylib.web.a

WEB_NAME = index.html
DATA_DIR = data

# ============================================================================
# PLATAFORMA
# ============================================================================

ifeq ($(OS),Windows_NT)

    CC = gcc
    CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -MMD -MP
    LDFLAGS = -L. -lraylib -lopengl32 -lgdi32 -lwinmm -lole32 -lws2_32 -lm

    MKDIR_P = if not exist "$(@D)" mkdir "$(@D)"
    RM = del /F /Q
    RMDIR = rmdir /S /Q
    CP = copy /Y

else

    CC = cc
    CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -MMD -MP
    LDFLAGS = -L. -lraylib -lm -ldl -lpthread -lGL -lX11 -g -fsanitize=address -fsanitize=leak

    MKDIR_P = mkdir -p "$(@D)"
    RM = rm -f
    RMDIR = rm -rf
    CP = cp -f

endif

# ============================================================================
# TARGETS
# ============================================================================

all: $(OBJ_DIR) $(RAYLIB_LIB) $(NAME)

test: $(RAYLIB_LIB) $(TEST_OBJ_DIR) $(TEST_NAME)
	./$(TEST_NAME)

web: $(WEB_RAYLIB_LIB)
	emcc $(SRC_DIR)/$(WEB_SRC) \
		-I$(INC_DIR) -I$(RAYLIB_DIR)/src \
		$(WEB_RAYLIB_LIB) \
		-o $(WEB_NAME) \
		-DPLATFORM_WEB \
		-s USE_GLFW=3 \
		-s ASYNCIFY \
		-s TOTAL_MEMORY=67108864 \
		-s FORCE_FILESYSTEM=1 \
		-s ALLOW_MEMORY_GROWTH=1 \
		--preload-file $(DATA_DIR) \
		--shell-file $(RAYLIB_DIR)/src/shell.html \
		-Os

# ============================================================================
# RAYLIB
# ============================================================================

$(RAYLIB_LIB):
ifeq ($(OS),Windows_NT)
	@if not exist "$(RAYLIB_DIR)" ( \
		echo Cloning raylib... & \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)" \
	)
	@cd "$(RAYLIB_DIR)/src" && $(MAKE) clean PLATFORM=PLATFORM_DESKTOP
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@$(CP) "$(RAYLIB_DIR)\src\libraylib.a" "$(RAYLIB_LIB)"
else
	@if [ ! -d "$(RAYLIB_DIR)" ]; then \
		echo "Cloning raylib..."; \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)"; \
	fi
	@cd "$(RAYLIB_DIR)/src" && $(MAKE) clean PLATFORM=PLATFORM_DESKTOP
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@$(CP) "$(RAYLIB_DIR)/src/libraylib.a" "$(RAYLIB_LIB)"
endif

$(WEB_RAYLIB_LIB):
ifeq ($(OS),Windows_NT)
	@if not exist "$(RAYLIB_DIR)" ( \
		echo Cloning raylib... & \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)" \
	)
	@cd "$(RAYLIB_DIR)/src" && $(MAKE) clean PLATFORM=PLATFORM_WEB
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_WEB RAYLIB_LIBTYPE=STATIC CC=emcc AR=emar
	@if exist "$(RAYLIB_DIR)\src\libraylib.web.a" $(CP) "$(RAYLIB_DIR)\src\libraylib.web.a" "$(WEB_RAYLIB_LIB)"
	@if exist "$(RAYLIB_DIR)\src\libraylib.a" $(CP) "$(RAYLIB_DIR)\src\libraylib.a" "$(WEB_RAYLIB_LIB)"
else
	@if [ ! -d "$(RAYLIB_DIR)" ]; then \
		echo "Cloning raylib..."; \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)"; \
	fi
	@cd "$(RAYLIB_DIR)/src" && $(MAKE) clean PLATFORM=PLATFORM_WEB
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_WEB RAYLIB_LIBTYPE=STATIC CC=emcc AR=emar
	@if [ -f "$(RAYLIB_DIR)/src/libraylib.web.a" ]; then $(CP) "$(RAYLIB_DIR)/src/libraylib.web.a" "$(WEB_RAYLIB_LIB)"; fi
	@if [ -f "$(RAYLIB_DIR)/src/libraylib.a" ]; then $(CP) "$(RAYLIB_DIR)/src/libraylib.a" "$(WEB_RAYLIB_LIB)"; fi
endif

# ============================================================================
# EXECUTABLES
# ============================================================================

$(NAME): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)

$(TEST_NAME): $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJ) $(LDFLAGS)

# ============================================================================
# COMPILACIÓN OBJETOS
# ============================================================================

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -c $< -o $@

$(TEST_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(MKDIR_P)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================================
# DIRECTORIOS
# ============================================================================

$(OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(OBJ_DIR)" mkdir "$(OBJ_DIR)"
else
	@mkdir -p "$(OBJ_DIR)"
endif

$(TEST_OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(TEST_OBJ_DIR)" mkdir "$(TEST_OBJ_DIR)"
else
	@mkdir -p "$(TEST_OBJ_DIR)"
endif

# ============================================================================
# CLEAN
# ============================================================================

clean:
ifeq ($(OS),Windows_NT)
	@if exist "$(OBJ_DIR)" $(RMDIR) "$(OBJ_DIR)"
	@if exist "$(TEST_OBJ_DIR)" $(RMDIR) "$(TEST_OBJ_DIR)"
else
	$(RMDIR) "$(OBJ_DIR)" "$(TEST_OBJ_DIR)"
endif

fclean: clean
ifeq ($(OS),Windows_NT)
	@if exist "$(NAME)" $(RM) "$(NAME)"
	@if exist "$(TEST_NAME)" $(RM) "$(TEST_NAME)"
	@if exist "$(RAYLIB_LIB)" $(RM) "$(RAYLIB_LIB)"
	@if exist "$(WEB_RAYLIB_LIB)" $(RM) "$(WEB_RAYLIB_LIB)"
	@if exist "index.html" $(RM) "index.html"
	@if exist "index.js" $(RM) "index.js"
	@if exist "index.wasm" $(RM) "index.wasm"
	@if exist "index.data" $(RM) "index.data"
else
	$(RM) "$(NAME)" "$(TEST_NAME)" "$(RAYLIB_LIB)" "$(WEB_RAYLIB_LIB)"
	$(RM) "index.html" "index.js" "index.wasm" "index.data"
endif

re: fclean all

.PHONY: all clean fclean re test web

-include $(OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)
