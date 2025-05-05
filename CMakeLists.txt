cmake_minimum_required(VERSION 3.16)  
project(database_editor)  

set(CMAKE_CXX_STANDARD 17)  

# Ищем необходимые библиотеки  
find_package(OpenGL REQUIRED)  
find_package(glfw3 REQUIRED)  
find_package(SQLite3 REQUIRED)  


set(IMGUI_DIR ../imgui)  # Путь к ImGui

# Добавляем исходники ImGui
file(GLOB IMGUI_SOURCES 
    "${IMGUI_DIR}/*.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)

# Основной исполняемый файл
add_executable(database_editor
    main.cpp  # Ваш основной файл с кодом
    database.cpp
    ${IMGUI_SOURCES}
)

# Подключаем зависимости
target_include_directories(database_editor  PRIVATE
    ${CMAKE_SOURCE_DIR}  #наши заголовки
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
    ${OPENGL_INCLUDE_DIR}    
    ${GLFW3_INCLUDE_DIR}  
    ${SQLite3_INCLUDE_DIR}  
) 

target_link_libraries(database_editor PRIVATE
 #    ${OPENGL_LIBRARIES}  
    glfw
    ${OPENGL_LIBRARIES}
    ${SQLite3_LIBRARY}  
)
