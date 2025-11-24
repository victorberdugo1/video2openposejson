NAME = raypup
TEST_NAME = test_viewer

# Directorios
INC_DIR = inc
SRC_DIR = src
OBJ_DIR = obj
TEST_OBJ_DIR = obj_test

# Archivos fuente
SRC = main.c bones3d.c
<<<<<<< HEAD
TEST_SRC = test.c bones3d.c
=======
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1

# Rutas completas de los fuentes
SRCS = $(addprefix $(SRC_DIR)/, $(SRC))
TEST_SRCS = $(addprefix $(SRC_DIR)/, $(TEST_SRC))

# Objetos
OBJ = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
TEST_OBJ = $(patsubst $(SRC_DIR)/%.c, $(TEST_OBJ_DIR)/%.o, $(TEST_SRCS))

# Raylib
RAYLIB_DIR = raylib
RAYLIB_LIB = libraylib.a

# =============================
# Compilador según sistema
# =============================
ifeq ($(OS),Windows_NT)
	# Windows
	CC = gcc
	CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -MMD -MP
	LDFLAGS = -L. -lraylib -lopengl32 -lgdi32 -lwinmm -lole32 -lws2_32 -lm
else
	# Linux / POSIX
	CC = cc
	CFLAGS = -Wall -Wextra -Werror -I$(INC_DIR) -I$(RAYLIB_DIR)/src -g -fsanitize=address -fsanitize=leak -MMD -MP
	LDFLAGS = -L. -lraylib -lm -ldl -lpthread -lGL -lX11
endif

# Colores
GREEN = \033[0;32m
RED = \033[0;91m
YELLOW = \033[0;33m
RESET = \033[0m

# Target principal
all: $(OBJ_DIR) $(RAYLIB_LIB) $(NAME)

# ============================
# Compilar Raylib si no existe
# ============================
$(RAYLIB_LIB):
<<<<<<< HEAD
ifeq ($(OS),Windows_NT)
	@rem Windows-friendly commands
	@echo $(RESET)Checking for $(RAYLIB_DIR)...
	@if not exist "$(RAYLIB_DIR)" git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)"
	@echo $(RESET)Building raylib...
	@$(RAYLIB_MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@copy /Y "$(RAYLIB_DIR)\src\$(RAYLIB_LIB)" .
else
	@echo $(RESET)Checking for $(RAYLIB_DIR)...
=======
	@echo "$(RESET)Checking for raylib..."
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1
	@if [ ! -d "$(RAYLIB_DIR)" ]; then \
		echo "Cloning raylib..."; \
		git clone https://github.com/raysan5/raylib.git "$(RAYLIB_DIR)"; \
	fi
	@echo "Building raylib..."
	@$(MAKE) -C "$(RAYLIB_DIR)/src" PLATFORM=PLATFORM_DESKTOP RAYLIB_LIBTYPE=STATIC
	@cp -f "$(RAYLIB_DIR)/src/$(RAYLIB_LIB)" .

# ============================
<<<<<<< HEAD
# Compilar ejecutable principal
# ============================
$(NAME): $(RAYLIB_LIB) $(OBJ)
=======
# Compilar programa principal
# ============================
$(NAME): $(OBJ)
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDFLAGS)
	@echo "$(GREEN)Executable $(NAME) created.$(RESET)"

# ============================
<<<<<<< HEAD
# Compilar ejecutable de test
# ============================
test: $(RAYLIB_LIB) $(TEST_OBJ_DIR) $(TEST_NAME)
	@echo "$(YELLOW)Running test viewer...$(RESET)"
	./$(TEST_NAME)

$(TEST_NAME): $(RAYLIB_LIB) $(TEST_OBJ)
	$(CC) $(CFLAGS) -o $@ $(TEST_OBJ) $(LDFLAGS)
	@echo "$(GREEN)Test executable $(TEST_NAME) successfully created.$(RESET)"

# ============================
# Reglas de compilación
# ============================

# Objetos principales
=======
# Compilar cada archivo .c
# ============================
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p "$(@D)"
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)Compiled: $<$(RESET)"

<<<<<<< HEAD
# Objetos de test
$(TEST_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
ifeq ($(OS),Windows_NT)
	@if not exist "$(@D)" mkdir "$(@D)"
else
	@mkdir -p "$(@D)"
endif
	$(CC) $(CFLAGS) -c $< -o $@
	@echo "$(GREEN)Compiled (test): $<$(RESET)"

# Asegurar directorios
=======
# ============================
# Crear carpeta obj si falta
# ============================
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1
$(OBJ_DIR):
	@mkdir -p "$(OBJ_DIR)"

<<<<<<< HEAD
$(TEST_OBJ_DIR):
ifeq ($(OS),Windows_NT)
	@if not exist "$(TEST_OBJ_DIR)" mkdir "$(TEST_OBJ_DIR)"
else
	@mkdir -p "$(TEST_OBJ_DIR)"
endif

=======
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1
# ============================
# Limpieza
# ============================
clean:
<<<<<<< HEAD
ifeq ($(OS),Windows_NT)
	@if exist "$(OBJ_DIR)" rmdir /S /Q "$(OBJ_DIR)"
	@if exist "$(TEST_OBJ_DIR)" rmdir /S /Q "$(TEST_OBJ_DIR)"
	@echo "$(RED)Object files deleted.$(RESET)"
else
	@rm -rf $(OBJ_DIR) $(TEST_OBJ_DIR)
	@echo "$(RED)Object files deleted.$(RESET)"
endif

fclean: clean
ifeq ($(OS),Windows_NT)
	@if exist "$(NAME)" del /Q "$(NAME)"
	@if exist "$(TEST_NAME)" del /Q "$(TEST_NAME)"
	@if exist "$(RAYLIB_LIB)" del /Q "$(RAYLIB_LIB)"
	@echo "$(RED)Executable and libraries deleted.$(RESET)"
else
	@rm -f $(NAME) $(TEST_NAME) $(RAYLIB_LIB)
	@echo "$(RED)Executable and libraries deleted.$(RESET)"
endif
=======
	@rm -rf "$(OBJ_DIR)"
	@echo "$(RED)Object files removed.$(RESET)"

fclean: clean
	@rm -f "$(NAME)" "$(RAYLIB_LIB)"
	@echo "$(RED)Executable and libraries removed.$(RESET)"
>>>>>>> 89e5a4119502b35ff71c2c019c2063fc30ec3fa1

re: fclean all

.PHONY: all clean fclean re test

# Incluir dependencias generadas automáticamente (.d)
-include $(OBJ:.o=.d)
-include $(TEST_OBJ:.o=.d)