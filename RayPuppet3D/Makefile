NAME = raypup
TEST_NAME = test_viewer

INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
TEST_OBJ_DIR = obj_test

SRC = main.c bones3d.c
TEST_SRC = test.c bones3d.c

SRCS = $(addprefix $(SRC_DIR)/, $(SRC))
TEST_SRCS = $(addprefix $(SRC_DIR)/, $(TEST_SRC))

OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TEST_OBJ = $(patsubst $(SRC_DIR)/%.c, $(TEST_OBJ_DIR)/%.o, $(TEST_SRCS))

RAYLIB_DIR = raylib
RAYLIB_LIB = libraylib.a

# ============================================================================
# PLATAFORMA
# ============================================================================

ifeq ($(OS),Windows_NT)

    CC = gcc
    CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -MMD -MP
    LDFLAGS = -L. -lraylib -lopengl32 -lgdi32 -lwinmm -lole32 -lws2_32 -lm

    MKDIR_P = if not exist "$(@D)" mkdir "$(@D)"
    MKDIR_DIR = if not exist "$(1)" mkdir "$(1)"
    RM = del /F /Q
    RMDIR = rmdir /S /Q
    CP = copy /Y

else

    CC = cc
    CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -MMD -MP
    LDFLAGS = -L. -lraylib -lm -ldl -lpthread -lGL -lX11

    MKDIR_P = mkdir -p "$(@D)"
    MKDIR_DIR = mkdir -p "$(1)"
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

# ============================================================================
# RAYLIB
# ============================================================================

$(RAYLIB_LIB):
ifeq ($(OS),Windows_NT)
	@if not exist "$(RAYLIB_DIR)" ( \
		echo Cloning raylib... & \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)" \
	)
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@$(CP) "$(RAYLIB_DIR)\src\$(RAYLIB_LIB)" .
else
	@if [ ! -d "$(RAYLIB_DIR)" ]; then \
		echo "Cloning raylib..."; \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)"; \
	fi
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@$(CP) "$(RAYLIB_DIR)/src/$(RAYLIB_LIB)" .
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
else
	$(RM) "$(NAME)" "$(TEST_NAME)" "$(RAYLIB_LIB)"
endif

re: fclean all

.PHONY: all clean fclean re test

-include $(OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)