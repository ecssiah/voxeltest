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

typedef u8 VisibilityMask;

#define FACE_POS_X (1 << 0)
#define FACE_NEG_X (1 << 1)
#define FACE_POS_Y (1 << 2)
#define FACE_NEG_Y (1 << 3)
#define FACE_POS_Z (1 << 4)
#define FACE_NEG_Z (1 << 5)

static const char* VISIBILITY_MASK_TO_STRING[6] = {
    "FACE_POS_X",
    "FACE_NEG_X",
    "FACE_POS_Y",
    "FACE_NEG_Y",
    "FACE_POS_Z",
    "FACE_NEG_Z",
};

typedef struct VertexData VertexData;
struct VertexData
{
    f32 position_array[3];
    f32 normal_array[3];
    f32 uv_array[2];
};

VertexData voxel_vertex_array[6][4] = {
    // FACE_POS_X
    {
	{{+CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS}, {+1, +0, +0}, {0, 0}},
	{{+CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS}, {+1, +0, +0}, {1, 0}},
	{{+CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS}, {+1, +0, +0}, {1, 1}},
	{{+CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS}, {+1, +0, +0}, {0, 1}},
    },

    // FACE_NEG_X
    {
	{{-CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS}, {-1, +0, +0}, {0, 0}},
	{{-CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS}, {-1, +0, +0}, {1, 0}},
	{{-CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS}, {-1, +0, +0}, {1, 1}},
	{{-CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS}, {-1, +0, +0}, {0, 1}},
    },

    // FACE_POS_Y
    {
	{{-CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS}, {+0, +1, +0}, {0, 0}},
	{{+CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS}, {+0, +1, +0}, {1, 0}},
	{{+CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS}, {+0, +1, +0}, {1, 1}},
	{{-CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS}, {+0, +1, +0}, {0, 1}},
    },

    // FACE_NEG_Y
    {
	{{-CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS}, {+0, -1, +0}, {0, 0}},
	{{+CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS}, {+0, -1, +0}, {1, 0}},
	{{+CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS}, {+0, -1, +0}, {1, 1}},
	{{-CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS}, {+0, -1, +0}, {0, 1}},
    },

    // FACE_POS_Z
    {
	{{-CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS}, {+0, +0, +1}, {0, 0}},
	{{-CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS}, {+0, +0, +1}, {0, 1}},
	{{+CELL_RADIUS, +CELL_RADIUS, +CELL_RADIUS}, {+0, +0, +1}, {1, 1}},
	{{+CELL_RADIUS, -CELL_RADIUS, +CELL_RADIUS}, {+0, +0, +1}, {1, 0}},
    },

    // FACE_NEG_Z
    {
	{{+CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS}, {0, 0, -1}, {0, 0}},
	{{+CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS}, {0, 0, -1}, {0, 1}},
	{{-CELL_RADIUS, +CELL_RADIUS, -CELL_RADIUS}, {0, 0, -1}, {1, 1}},
	{{-CELL_RADIUS, -CELL_RADIUS, -CELL_RADIUS}, {0, 0, -1}, {1, 0}},
    }
};

typedef struct GpuMesh GpuMesh;
struct GpuMesh
{
    VertexData* vertex_array;
    u32* index_array;

    size_t vertex_count;
    size_t index_count;
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

typedef struct SectorFace SectorFace;
struct SectorFace
{
    vec3 position;
    u8 face;
    u8 block_type;
};

typedef struct SectorMesh SectorMesh;
struct SectorMesh
{
    SectorFace sector_face_array[SECTOR_VOLUME_IN_VERTICES];

    u32 count;
};

typedef enum EBlockType EBlockType;
enum EBlockType
{
    BlockType_None,
    BlockType_Lion,
    BlockType_Eagle,
    BlockType_Wolf,
    BlockType_Horse,

    BlockType_COUNT,
};

static const char* BLOCK_TYPE_TO_STRING[BlockType_COUNT] = {
    "BlockType_None",
    "BlockType_Lion",
    "BlockType_Eagle",
    "BlockType_Wolf",
    "BlockType_Horse"
};

typedef struct Cell Cell;
struct Cell
{
    u32 sector_index;
    u32 cell_index;
    
    EBlockType block_type;
    VisibilityMask visibility_mask;
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

    return cell->block_type != BlockType_None;
}

VisibilityMask map_compute_visibility_at(u32 x, u32 y, u32 z)
{
    VisibilityMask mask = 0;

    if (!map_is_solid(x + 1, y, z)) mask |= FACE_POS_X;
    if (!map_is_solid(x - 1, y, z)) mask |= FACE_NEG_X;

    if (!map_is_solid(x, y + 1, z)) mask |= FACE_POS_Y;
    if (!map_is_solid(x, y - 1, z)) mask |= FACE_NEG_Y;

    if (!map_is_solid(x, y, z + 1)) mask |= FACE_POS_Z;
    if (!map_is_solid(x, y, z - 1)) mask |= FACE_NEG_Z;

    return mask;
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
	    cell->visibility_mask = 0;

	    EBlockType block_type = (rand() % BlockType_COUNT);
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

	    cell->visibility_mask = map_compute_visibility_at(
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

void render_emit_sector_mesh_face(ivec3 world_coordinate, VisibilityMask face_mask, EBlockType block_type)
{
    u32 sector_index = map_world_coordinate_to_sector_index(world_coordinate);

    SectorMesh* sector_mesh = &sector_mesh_array[sector_index];
    SectorFace* sector_face = &sector_mesh->sector_face_array[sector_mesh->count];

    sector_face->face = face_mask;
    sector_face->block_type = block_type;
    sector_face->position[0] = (f32)world_coordinate[0];
    sector_face->position[1] = (f32)world_coordinate[1];
    sector_face->position[2] = (f32)world_coordinate[2];

    sector_mesh->count += 1;
}

void render_generate_sector_mesh(u32 sector_index)
{
    u32 cell_index;
    ivec3 world_coordinate;
    
    Sector* sector = &world.sector_array[sector_index];

    for (cell_index = 0; cell_index < SECTOR_VOLUME_IN_CELLS; ++cell_index)
    {
	Cell* cell = &sector->cell_array[cell_index];

	if (cell->block_type == BlockType_None)
	{
	    continue;
	}
	    
	map_indices_to_world_coordinate(sector_index, cell_index, world_coordinate);

	if (cell->visibility_mask & FACE_POS_X)
	{
	    render_emit_sector_mesh_face(world_coordinate, FACE_POS_X, cell->block_type);
	}

	if (cell->visibility_mask & FACE_NEG_X)
	{
	    render_emit_sector_mesh_face(world_coordinate, FACE_NEG_X, cell->block_type);	    
	}

	if (cell->visibility_mask & FACE_POS_Y)
	{
	    render_emit_sector_mesh_face(world_coordinate, FACE_POS_Y, cell->block_type);	    
	}

	if (cell->visibility_mask & FACE_NEG_Y)
	{
	    render_emit_sector_mesh_face(world_coordinate, FACE_NEG_Y, cell->block_type);	    
	}

	if (cell->visibility_mask & FACE_POS_Z)
	{
	    render_emit_sector_mesh_face(world_coordinate, FACE_POS_Z, cell->block_type);	    
	}

	if (cell->visibility_mask & FACE_NEG_Z)
	{
	    render_emit_sector_mesh_face(world_coordinate, FACE_NEG_Z, cell->block_type);	    
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
