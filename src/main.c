#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "jsk.h"
#include "jsk_gl.h"

#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define WINDOW_TITLE "OpenGL Test"

#define CELL_RADIUS 0.5f
#define CELL_SIZE (2.0f * CELL_RADIUS)
#define CELL_VOLUME (CELL_SIZE * CELL_SIZE * CELL_SIZE)

#define WORLD_SIZE_IN_SECTORS_LOG2 1
#define WORLD_SIZE_IN_SECTORS (1 << WORLD_SIZE_IN_SECTORS_LOG2)
#define WORLD_VOLUME_IN_SECTORS (1 << (3 * WORLD_SIZE_IN_SECTORS_LOG2))

#define WORLD_STRIDE_IN_SECTORS_X 1
#define WORLD_STRIDE_IN_SECTORS_Y (1 << WORLD_SIZE_IN_SECTORS_LOG2)
#define WORLD_STRIDE_IN_SECTORS_Z (1 << (2 * WORLD_SIZE_IN_SECTORS_LOG2))

#define WORLD_SIZE_MASK_IN_SECTORS (WORLD_SIZE_IN_SECTORS - 1)

#define SECTOR_SIZE_IN_CELLS_LOG2 1
#define SECTOR_SIZE_IN_CELLS (1 << SECTOR_SIZE_IN_CELLS_LOG2)
#define SECTOR_VOLUME_IN_CELLS (1 << (3 * SECTOR_SIZE_IN_CELLS_LOG2))

#define SECTOR_STRIDE_IN_CELLS_X 1
#define SECTOR_STRIDE_IN_CELLS_Y (1 << SECTOR_SIZE_IN_CELLS_LOG2)
#define SECTOR_STRIDE_IN_CELLS_Z (1 << (2 * SECTOR_SIZE_IN_CELLS_LOG2))

#define SECTOR_SIZE_MASK_IN_CELLS (SECTOR_SIZE_IN_CELLS - 1)

#define WORLD_SIZE_IN_CELLS (WORLD_SIZE_IN_SECTORS * SECTOR_SIZE_IN_CELLS)
#define WORLD_VOLUME_IN_CELLS (WORLD_SIZE_IN_CELLS * WORLD_SIZE_IN_CELLS * WORLD_SIZE_IN_CELLS)

#define SECTOR_VOLUME_IN_VERTICES (SECTOR_VOLUME_IN_CELLS * 6)

typedef enum ECellFace ECellFace;
enum ECellFace
{
    CELL_FACE_POS_X = 0,
    CELL_FACE_NEG_X,
    CELL_FACE_POS_Y,
    CELL_FACE_NEG_Y,
    CELL_FACE_POS_Z,
    CELL_FACE_NEG_Z,

    CELL_FACE_COUNT,
};

static const char* CELL_FACE_TO_STRING[CELL_FACE_COUNT] = {
    "CELL_FACE_POS_X",
    "CELL_FACE_NEG_X",
    "CELL_FACE_POS_Y",
    "CELL_FACE_NEG_Y",
    "CELL_FACE_POS_Z",
    "CELL_FACE_NEG_Z",
};

typedef u8 CellFaceMask;

#define CELL_FACE_MASK_POS_X ((CellFaceMask)(1 << CELL_FACE_POS_X))
#define CELL_FACE_MASK_NEG_X ((CellFaceMask)(1 << CELL_FACE_NEG_X))
#define CELL_FACE_MASK_POS_Y ((CellFaceMask)(1 << CELL_FACE_POS_Y))
#define CELL_FACE_MASK_NEG_Y ((CellFaceMask)(1 << CELL_FACE_NEG_Y))
#define CELL_FACE_MASK_POS_Z ((CellFaceMask)(1 << CELL_FACE_POS_Z))
#define CELL_FACE_MASK_NEG_Z ((CellFaceMask)(1 << CELL_FACE_NEG_Z))

typedef enum EBlockType EBlockType;
enum EBlockType
{
    BLOCK_TYPE_NONE = 0,
    BLOCK_TYPE_LION,
    BLOCK_TYPE_EAGLE,
    BLOCK_TYPE_WOLF,
    BLOCK_TYPE_HORSE,

    BLOCK_TYPE_COUNT,
};

static const char* BLOCK_TYPE_TO_STRING[BLOCK_TYPE_COUNT] = {
    "BLOCK_TYPE_NONE",
    "BLOCK_TYPE_LION",
    "BLOCK_TYPE_EAGLE",
    "BLOCK_TYPE_WOLF",
    "BLOCK_TYPE_HORSE"
};

typedef struct VertexData VertexData;
struct VertexData
{
    f32 position_array[3];
    f32 normal_array[3];
    f32 uv_array[2];
};

static f32 CELL_FACE_VERTEX_ARRAY[CELL_FACE_COUNT][4][3] = {
    // CELL_FACE_POS_X
    {
	{+CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS},
	{+CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS},
	{+CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS},
	{+CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS},
    },
    
    // CELL_FACE_NEG_X
    {
        {-CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS},
        {-CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS},
        {-CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS},
        {-CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS},
    },

    // CELL_FACE_POS_Y
    {
        {-CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS},
        {+CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS},
        {+CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS},
        {-CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS},
    },

    // CELL_FACE_NEG_Y
    {
        {-CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS},
        {+CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS},
        {+CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS},
        {-CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS},
    },

    // CELL_FACE_POS_Z
    {
        {-CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS},
        {-CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS},
        {+CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS},
        {+CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS},
    },

    // CELL_FACE_NEG_Z
    {
        {+CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS},
        {+CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS},
        {-CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS},
        {-CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS},
    },
};

static f32 CELL_FACE_NORMAL_ARRAY[CELL_FACE_COUNT][3] = {
    // CELL_FACE_POS_X
    { +1.0f, +0.0f, +0.0f },

    // CELL_FACE_NEG_X
    { -1.0f, +0.0f, +0.0f },

    // CELL_FACE_POS_Y
    { +0.0f, +1.0f, +0.0f },

    // CELL_FACE_NEG_Y
    { +0.0f, -1.0f, +0.0f },

    // CELL_FACE_POS_Z
    { +0.0f, +0.0f, +1.0f },

    // CELL_FACE_NEG_Z
    { +0.0f, +0.0f, -1.0f },
};

static f32 CELL_FACE_UV_ARRAY[CELL_FACE_COUNT][4][2] = {
    // CELL_FACE_POS_X
    {
	{+0.0f, +0.0f},
	{+1.0f, +0.0f},
	{+1.0f, +1.0f},
	{+0.0f, +1.0f},
    },

    // CELL_FACE_NEG_X
    {
	{+0.0f, +0.0f},
	{+1.0f, +0.0f},
	{+1.0f, +1.0f},
	{+0.0f, +1.0f},
    },

    // CELL_FACE_POS_Y
    {
	{+0.0f, +0.0f},
	{+1.0f, +0.0f},
	{+1.0f, +1.0f},
	{+0.0f, +1.0f},
    },

    // CELL_FACE_NEG_Y
    {
	{+0.0f, +0.0f},
	{+1.0f, +0.0f},
	{+1.0f, +1.0f},
	{+0.0f, +1.0f},
    },

    // CELL_FACE_POS_Z
    {
	{+0.0f, +0.0f},
	{+1.0f, +0.0f},
	{+1.0f, +1.0f},
	{+0.0f, +1.0f},
    },

    // CELL_FACE_NEG_Z
    {
	{+0.0f, +0.0f},
	{+1.0f, +0.0f},
	{+1.0f, +1.0f},
	{+0.0f, +1.0f},
    },
};

typedef struct GpuMesh GpuMesh;
struct GpuMesh
{
    VertexData* vertex_array;
    u32* index_array;

    size_t vertex_count;
    size_t index_count;
};

typedef struct SectorFace SectorFace;
struct SectorFace
{
    vec3 world_position;
    
    ECellFace cell_face;
    EBlockType block_type;
};

typedef struct SectorMesh SectorMesh;
struct SectorMesh
{
    SectorFace sector_face_array[SECTOR_VOLUME_IN_VERTICES];

    u32 count;
};

typedef struct GLContext GLContext;
struct GLContext
{
    GLFWwindow* window;

    GLuint program_id;
    GLuint vao_id;
    GLuint vbo_id;

    GLuint texture_id;
};

typedef struct Cell Cell;
struct Cell
{
    u32 sector_index;
    u32 cell_index;
    
    EBlockType block_type;
    CellFaceMask cell_face_mask;
};

typedef struct Sector Sector;
struct Sector
{
    u32 sector_index;
    
    Cell cell_array[SECTOR_VOLUME_IN_CELLS];
};

typedef struct World World;
struct World
{
    Sector sector_array[WORLD_VOLUME_IN_SECTORS];
};

static World world;

static GLContext gl_context;
static SectorMesh sector_mesh_array[SECTOR_VOLUME_IN_CELLS];

u32 map_sector_coordinate_to_index(ivec3 sector_coordinate)
{
    assert(sector_coordinate[0] >= 0 && sector_coordinate[0] < WORLD_SIZE_IN_SECTORS);
    assert(sector_coordinate[1] >= 0 && sector_coordinate[1] < WORLD_SIZE_IN_SECTORS);
    assert(sector_coordinate[2] >= 0 && sector_coordinate[2] < WORLD_SIZE_IN_SECTORS);
    
    u32 sector_index =
	sector_coordinate[0] +
	(sector_coordinate[1] << WORLD_SIZE_IN_SECTORS_LOG2) +
	(sector_coordinate[2] << (2 * WORLD_SIZE_IN_SECTORS_LOG2));
	    
    return sector_index;
}

void map_sector_index_to_coordinate(u32 sector_index, ivec3 out_sector_coordinate)
{
    assert(sector_index < WORLD_VOLUME_IN_SECTORS);
    
    out_sector_coordinate[0] = sector_index & WORLD_SIZE_MASK_IN_SECTORS;
    out_sector_coordinate[1] = (sector_index >> WORLD_SIZE_IN_SECTORS_LOG2) & WORLD_SIZE_MASK_IN_SECTORS;
    out_sector_coordinate[2] = sector_index >> (2 * WORLD_SIZE_IN_SECTORS_LOG2);
}

u32 map_cell_coordinate_to_index(ivec3 cell_coordinate)
{
    assert(cell_coordinate[0] >= 0 && cell_coordinate[0] < SECTOR_SIZE_IN_CELLS);
    assert(cell_coordinate[1] >= 0 && cell_coordinate[1] < SECTOR_SIZE_IN_CELLS);
    assert(cell_coordinate[2] >= 0 && cell_coordinate[2] < SECTOR_SIZE_IN_CELLS);
    
    u32 cell_index =
	cell_coordinate[0] +
	(cell_coordinate[1] << SECTOR_SIZE_IN_CELLS_LOG2) +
	(cell_coordinate[2] << (2 * SECTOR_SIZE_IN_CELLS_LOG2));
	    
    return cell_index;
}

void map_cell_index_to_coordinate(u32 cell_index, ivec3 out_cell_coordinate)
{
    assert(cell_index < SECTOR_VOLUME_IN_CELLS);
    
    out_cell_coordinate[0] = cell_index & SECTOR_SIZE_MASK_IN_CELLS;
    out_cell_coordinate[1] = (cell_index >> SECTOR_SIZE_IN_CELLS_LOG2) & SECTOR_SIZE_MASK_IN_CELLS;
    out_cell_coordinate[2] = cell_index >> (2 * SECTOR_SIZE_IN_CELLS_LOG2);
}

u32 map_world_coordinate_to_sector_index(ivec3 world_coordinate)
{
    assert(world_coordinate[0] < WORLD_SIZE_IN_CELLS);
    assert(world_coordinate[1] < WORLD_SIZE_IN_CELLS);
    assert(world_coordinate[2] < WORLD_SIZE_IN_CELLS);
    
    ivec3 sector_coordinate;

    sector_coordinate[0] = world_coordinate[0] >> SECTOR_SIZE_IN_CELLS_LOG2;
    sector_coordinate[1] = world_coordinate[1] >> SECTOR_SIZE_IN_CELLS_LOG2;
    sector_coordinate[2] = world_coordinate[2] >> SECTOR_SIZE_IN_CELLS_LOG2;

    u32 sector_index = map_sector_coordinate_to_index(sector_coordinate);

    return sector_index;
}

u32 map_world_coordinate_to_cell_index(ivec3 world_coordinate)
{
    assert(world_coordinate[0] < WORLD_SIZE_IN_CELLS);
    assert(world_coordinate[1] < WORLD_SIZE_IN_CELLS);
    assert(world_coordinate[2] < WORLD_SIZE_IN_CELLS);
    
    ivec3 cell_coordinate;

    cell_coordinate[0] = (u32)world_coordinate[0] & SECTOR_SIZE_MASK_IN_CELLS;
    cell_coordinate[1] = (u32)world_coordinate[1] & SECTOR_SIZE_MASK_IN_CELLS;
    cell_coordinate[2] = (u32)world_coordinate[2] & SECTOR_SIZE_MASK_IN_CELLS;

    u32 cell_index = map_cell_coordinate_to_index(cell_coordinate);

    return cell_index;
}

void map_indices_to_world_coordinate(u32 sector_index, u32 cell_index, ivec3 out_world_coordinate)
{
    ivec3 sector_coordinate;
    ivec3 cell_coordinate;
    
    map_sector_index_to_coordinate(sector_index, sector_coordinate);
    map_cell_index_to_coordinate(cell_index, cell_coordinate);

    glm_ivec3_scale(sector_coordinate, SECTOR_SIZE_IN_CELLS, out_world_coordinate);
    glm_ivec3_add(out_world_coordinate, cell_coordinate, out_world_coordinate);
}

void map_world_coordinate_to_position(ivec3 world_coordinate, vec3 out_world_position)
{
    out_world_position[0] = (f32)world_coordinate[0];
    out_world_position[1] = (f32)world_coordinate[1];
    out_world_position[1] = (f32)world_coordinate[2];
}

boolean map_is_solid(u32 x, u32 y, u32 z)
{   
    if (x >= WORLD_SIZE_IN_CELLS || y >= WORLD_SIZE_IN_CELLS || z >= WORLD_SIZE_IN_CELLS)
    {
	return False;
    }

    ivec3 world_coordinate = {x, y, z};
    
    u32 sector_index = map_world_coordinate_to_sector_index(world_coordinate);
    u32 cell_index = map_world_coordinate_to_cell_index(world_coordinate);
    
    Sector* sector = &world.sector_array[sector_index];
    Cell* cell = &sector->cell_array[cell_index];

    return cell->block_type != BLOCK_TYPE_NONE;
}

CellFaceMask map_get_cell_face_mask(u32 x, u32 y, u32 z)
{
    CellFaceMask cell_face_mask = 0;

    if (!map_is_solid(x + 1, y, z)) cell_face_mask |= CELL_FACE_MASK_POS_X;
    if (!map_is_solid(x - 1, y, z)) cell_face_mask |= CELL_FACE_MASK_NEG_X;

    if (!map_is_solid(x, y + 1, z)) cell_face_mask |= CELL_FACE_MASK_POS_Y;
    if (!map_is_solid(x, y - 1, z)) cell_face_mask |= CELL_FACE_MASK_NEG_Y;

    if (!map_is_solid(x, y, z + 1)) cell_face_mask |= CELL_FACE_MASK_POS_Z;
    if (!map_is_solid(x, y, z - 1)) cell_face_mask |= CELL_FACE_MASK_NEG_Z;

    return cell_face_mask;
}

void map_init()
{
    srand((u32)time(NULL));

    u32 sector_index;
    u32 cell_index;
    
    for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
    {
	Sector* sector = &world.sector_array[sector_index];
	sector->sector_index = sector_index;

	for (cell_index = 0; cell_index < SECTOR_VOLUME_IN_CELLS; ++cell_index)
	{
	    Cell* cell = &sector->cell_array[cell_index];
	    cell->sector_index = sector_index;
	    cell->cell_index = cell_index;
	    cell->cell_face_mask = 0;

	    EBlockType block_type = (EBlockType)(rand() % BLOCK_TYPE_COUNT);
	    cell->block_type = block_type;
	}
    }

    ivec3 world_coordinate;
    
    for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
    {
	Sector* sector = &world.sector_array[sector_index];
	
	for (cell_index = 0; cell_index < SECTOR_VOLUME_IN_CELLS; ++cell_index)
	{
	    Cell* cell = &sector->cell_array[cell_index];
	    
	    map_indices_to_world_coordinate(sector_index, cell_index, world_coordinate);

	    cell->cell_face_mask = map_get_cell_face_mask(
		world_coordinate[0],
		world_coordinate[1],
		world_coordinate[2]
	    );
	}
    }
}

void setup_opengl()
{
    int glfw_result = glfwInit();
    
    assert(glfw_result != 0);
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    gl_context.window = glfwCreateWindow(WINDOW_WIDTH,	WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);

    assert(gl_context.window != 0);

    glfwMakeContextCurrent(gl_context.window);

    int glad_load_gl_result = gladLoadGL();

    assert(glad_load_gl_result != 0);

    char* vert_src = read_file("assets/shaders/test.vert");
    char* frag_src = read_file("assets/shaders/test.frag");
    
    GLuint vert_shader = compile_shader(GL_VERTEX_SHADER, vert_src);
    GLuint frag_shader = compile_shader(GL_FRAGMENT_SHADER, frag_src);

    gl_context.program_id = glCreateProgram();
    glAttachShader(gl_context.program_id, vert_shader);
    glAttachShader(gl_context.program_id, frag_shader);
    glLinkProgram(gl_context.program_id);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    free(vert_src);
    free(frag_src);

    u32 index_array[] = {
	0, 1, 2,
	2, 3, 0
    };

    glGenVertexArrays(1, &gl_context.vao_id);
    glGenBuffers(1, &gl_context.vbo_id);

    glBindVertexArray(gl_context.vao_id);

    glBindBuffer(GL_ARRAY_BUFFER, gl_context.vbo_id);

    glVertexAttribPointer(
	0,
	3,
	GL_FLOAT,
	GL_FALSE,
	sizeof(VertexData),
	(void*)0
    );

    glVertexAttribPointer(
	1,
	2,
	GL_FLOAT,
	GL_FALSE,
	sizeof(VertexData),
	(void*)(offsetof(VertexData, uv_array))
    );

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    
    glBufferData(GL_ARRAY_BUFFER, sizeof(voxel_vertex_array), voxel_vertex_array, GL_STATIC_DRAW);

    int width;
    int height;
    int channels;
    
    unsigned char* pixel_data_array = stbi_load("assets/textures/lion.png", &width, &height, &channels, 4);

    glGenTextures(1, &gl_context.texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_context.texture_id);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data_array);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, gl_context.texture_id);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}

void render_emit_sector_mesh_face(ivec3 world_coordinate, CellFaceMask cell_face_mask, EBlockType block_type)
{

}

void render_generate_sector_mesh(u32 sector_index)
{
    u32 cell_index;
    ivec3 world_coordinate;
    
    Sector* sector = &world.sector_array[sector_index];

    SectorMesh* sector_mesh = &sector_mesh_array[sector->sector_index];
    sector_mesh->count = 0;

    for (cell_index = 0; cell_index < SECTOR_VOLUME_IN_CELLS; ++cell_index)
    {
	Cell* cell = &sector->cell_array[cell_index];

	if (cell->block_type == BLOCK_TYPE_NONE)
	{
	    continue;
	}
	    
	map_indices_to_world_coordinate(sector_index, cell_index, world_coordinate);

	vec3 world_position;
	map_world_coordinate_to_position(world_coordinate, world_position);

	CellFaceMask cell_face_mask_test = cell->cell_face_mask;

	while (cell_face_mask_test)
	{
	    ECellFace cell_face = (ECellFace)__builtin_ctz(cell_face_mask_test);

	    SectorFace* sector_face = &sector_mesh->sector_face_array[sector_mesh->count];

	    sector_face->cell_face = cell_face;
	    sector_face->block_type = cell->block_type;
	    glm_vec3_copy(world_position, sector_face->world_position);

	    sector_mesh->count += 1;

	    cell_face_mask_test &= cell_face_mask_test - 1;
	}
    }
}

void render_upload_sector_mesh_to_gpu(u32 sector_index)
{
    
}

void render_init()
{
    u32 sector_index;
    
    for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
    {
	render_generate_sector_mesh(sector_index);
    }

    for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
    {
	render_upload_sector_mesh_to_gpu(sector_index);
    }
}

int main()
{
    setup_opengl();
    
    map_init();

    render_init();
    
    while (!glfwWindowShouldClose(gl_context.window))
    {
	if (glfwGetKey(gl_context.window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
	{
	    glfwSetWindowShouldClose(gl_context.window, 1);
	}
	
	glClearColor(0.1f, 0.2f, 0.3f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(gl_context.program_id);
        glBindVertexArray(gl_context.vao_id);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(gl_context.window);
        glfwPollEvents();
    }

    glfwTerminate();
}
