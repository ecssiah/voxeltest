#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <cglm/cglm.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "jsk.h"
#include "jsk_log.h"
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

#define GET_CELL_FACE(cell_face_mask) ((ECellFace)(__builtin_ctz(cell_face_mask)))

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

#define VERTICES_PER_FACE 4
#define INDICES_PER_FACE 6

static f32 CELL_FACE_VERTEX_ARRAY[CELL_FACE_COUNT][VERTICES_PER_FACE][3] = {
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

static f32 CELL_FACE_UV_ARRAY[CELL_FACE_COUNT][VERTICES_PER_FACE][3] = {
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
    GLuint vao_id;
    GLuint vbo_id;
    GLuint ebo_id;
    
    VertexData* vertex_array;
    
    u32 vertex_count;
    u32 vertex_capacity;

    u32* index_array;
    
    u32 index_count;
    u32 index_capacity;
};

typedef struct SectorFace SectorFace;
struct SectorFace
{
    ivec3 cell_coordinate;
    
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
static GpuMesh gpu_mesh_array[SECTOR_VOLUME_IN_CELLS];

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
    u32 seed = 813;
    
    /* u32 seed = (u32)time(NULL); */
    
    srand(seed);

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

    GLint texture_sampler_location = glGetUniformLocation(gl_context.program_id, "u_texture_sampler");
    glUniform1i(texture_sampler_location, 0);

    glDeleteShader(vert_shader);
    glDeleteShader(frag_shader);

    free(vert_src);
    free(frag_src);

    int width;
    int height;
    int channels;
    
    unsigned char* pixel_data_array = stbi_load("assets/textures/lion.png", &width, &height, &channels, 4);

    glGenTextures(1, &gl_context.texture_id);
    glBindTexture(GL_TEXTURE_2D, gl_context.texture_id);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel_data_array);
    glGenerateMipmap(GL_TEXTURE_2D);
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

	ivec3 cell_coordinate;
	map_cell_index_to_coordinate(cell_index, cell_coordinate);
	    
	CellFaceMask cell_face_mask_test = cell->cell_face_mask;

	while (cell_face_mask_test)
	{
	    ECellFace cell_face = GET_CELL_FACE(cell_face_mask_test);

	    SectorFace* sector_face = &sector_mesh->sector_face_array[sector_mesh->count];

	    sector_face->cell_face = cell_face;
	    sector_face->block_type = cell->block_type;
	    
	    glm_ivec3_copy(cell_coordinate, sector_face->cell_coordinate);

	    sector_mesh->count += 1;

	    cell_face_mask_test &= cell_face_mask_test - 1;
	}
    }
}

void render_emit_sector_face(SectorFace* sector_face, GpuMesh* gpu_mesh)
{
    vec3 local_position;
    local_position[0] = (f32)sector_face->cell_coordinate[0];
    local_position[1] = (f32)sector_face->cell_coordinate[1];
    local_position[2] = (f32)sector_face->cell_coordinate[2];

    const f32* normal_array = CELL_FACE_NORMAL_ARRAY[sector_face->cell_face];

    u32 base_index = gpu_mesh->vertex_count;
    
    u32 vertex_index;
    for (vertex_index = 0; vertex_index < VERTICES_PER_FACE; ++vertex_index)
    {
	const f32* vertex_array = CELL_FACE_VERTEX_ARRAY[sector_face->cell_face][vertex_index];
	const f32* uv_array = CELL_FACE_UV_ARRAY[sector_face->cell_face][vertex_index];

	VertexData vertex_data;
	vertex_data.position_array[0] = local_position[0] + vertex_array[0];
	vertex_data.position_array[1] = local_position[1] + vertex_array[1];
	vertex_data.position_array[2] = local_position[2] + vertex_array[2];

	vertex_data.normal_array[0] = normal_array[0];
	vertex_data.normal_array[1] = normal_array[1];
	vertex_data.normal_array[2] = normal_array[2];

	vertex_data.uv_array[0] = uv_array[0];
	vertex_data.uv_array[1] = uv_array[1];

	gpu_mesh->vertex_array[gpu_mesh->vertex_count] = vertex_data;

	gpu_mesh->vertex_count += 1;
    }

    gpu_mesh->index_array[gpu_mesh->index_count + 0] = base_index + 0;
    gpu_mesh->index_array[gpu_mesh->index_count + 1] = base_index + 1;
    gpu_mesh->index_array[gpu_mesh->index_count + 2] = base_index + 2;
    gpu_mesh->index_array[gpu_mesh->index_count + 3] = base_index + 0;
    gpu_mesh->index_array[gpu_mesh->index_count + 4] = base_index + 2;
    gpu_mesh->index_array[gpu_mesh->index_count + 5] = base_index + 3;

    gpu_mesh->index_count += INDICES_PER_FACE;
}

void render_convert_sector_mesh_to_gpu_mesh(u32 sector_index)
{
    LOG_INFO("Sector Index: %u", sector_index);
    
    SectorMesh* sector_mesh = &sector_mesh_array[sector_index];
    GpuMesh* gpu_mesh = &gpu_mesh_array[sector_index];

    gpu_mesh->vertex_count = 0;
    gpu_mesh->index_count = 0;

    u32 required_vertex_capacity = sector_mesh->count * VERTICES_PER_FACE;
    u32 required_index_capacity = sector_mesh->count * INDICES_PER_FACE;

    if (gpu_mesh->vertex_capacity < required_vertex_capacity)
    {
	VertexData* new_vertex_data = malloc(sizeof(VertexData) * required_vertex_capacity);

	assert(new_vertex_data);
	
	gpu_mesh->vertex_capacity = required_vertex_capacity;
	gpu_mesh->vertex_array = new_vertex_data;
    }

    if (gpu_mesh->index_capacity < required_index_capacity)
    {
	u32* new_index_data = malloc(sizeof(u32) * required_index_capacity);

	assert(new_index_data);
	
	gpu_mesh->index_capacity = required_index_capacity;
	gpu_mesh->index_array = new_index_data;
    }

    LOG_INFO("GpuMesh Capacities Updated");
    
    u32 face_index;
    for (face_index = 0; face_index < sector_mesh->count; ++face_index)
    {
	SectorFace* sector_face = &sector_mesh->sector_face_array[face_index];

	render_emit_sector_face(sector_face, gpu_mesh);
    }

    LOG_INFO("Sector Faces Emitted");
    
    glGenVertexArrays(1, &gpu_mesh->vao_id);
    
    glGenBuffers(1, &gpu_mesh->vbo_id);
    glGenBuffers(1, &gpu_mesh->ebo_id);

    glBindVertexArray(gpu_mesh->vao_id);
    glBindBuffer(GL_ARRAY_BUFFER, gpu_mesh->vbo_id);

    glVertexAttribPointer(
	0,
	3,
	GL_FLOAT,
	GL_FALSE,
	sizeof(VertexData),
	(void*)0
    );

    glVertexAttribPointer(
	0,
	3,
	GL_FLOAT,
	GL_FALSE,
	sizeof(VertexData),
	(void*)(offsetof(VertexData, normal_array))
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
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    LOG_INFO("GL Buffers Setup");
}

void render_upload_gpu_mesh(u32 sector_index)
{
    GpuMesh* gpu_mesh = &gpu_mesh_array[sector_index];

    glBindBuffer(GL_ARRAY_BUFFER, gpu_mesh->vbo_id);

    glBufferData(
	GL_ARRAY_BUFFER,
	gpu_mesh->vertex_count * sizeof(VertexData),
	gpu_mesh->vertex_array,
	GL_STATIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu_mesh->ebo_id);

    glBufferData(
	GL_ELEMENT_ARRAY_BUFFER,
	gpu_mesh->index_count * sizeof(u32),
	gpu_mesh->index_array,
	GL_STATIC_DRAW
    );
}

void render_init()
{
    setup_opengl();

    LOG_INFO("OpenGL Setup");
    
    u32 sector_index;
    
    for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
    {
	render_generate_sector_mesh(sector_index);
    }

    LOG_INFO("Sector Meshes Generated");

    for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
    {
	render_convert_sector_mesh_to_gpu_mesh(sector_index);
	render_upload_gpu_mesh(sector_index);
    }

    LOG_INFO("Gpu Meshes Generated");
}

int main()
{
    log_init();
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

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl_context.texture_id);

	u32 sector_index;
	for (sector_index = 0; sector_index < WORLD_VOLUME_IN_SECTORS; ++sector_index)
	{
	    GpuMesh* gpu_mesh = &gpu_mesh_array[sector_index];

	    glDrawElements(
		GL_TRIANGLES,
		gpu_mesh->index_count,
		GL_UNSIGNED_INT,
		0
	    );
	}

        glfwSwapBuffers(gl_context.window);
        glfwPollEvents();
    }

    glfwTerminate();

    log_destroy();
}
