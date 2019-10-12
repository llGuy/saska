/* world.hpp */

#pragma once

#include "core.hpp"
#include "vulkan.hpp"
#include "graphics.hpp"
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>

#define VOXEL_CHUNK_EDGE_LENGTH 16
#define MAX_VERTICES_PER_VOXEL_CHUNK 5 * (VOXEL_CHUNK_EDGE_LENGTH - 1) * (VOXEL_CHUNK_EDGE_LENGTH - 1) * (VOXEL_CHUNK_EDGE_LENGTH - 1)

// Will always be allocated on the heap
struct voxel_chunk_t
{
    ivector3_t xs_bottom_corner;
    ivector3_t chunk_coord;
    
    uint8_t voxels[VOXEL_CHUNK_EDGE_LENGTH][VOXEL_CHUNK_EDGE_LENGTH][VOXEL_CHUNK_EDGE_LENGTH];

    uint32_t vertex_count;
    vector3_t mesh_vertices[MAX_VERTICES_PER_VOXEL_CHUNK];

    // Chunk rendering data
    mesh_t gpu_mesh;
    gpu_buffer_t chunk_mesh_gpu_buffer;

    struct push_k // Rendering stuff
    {
        matrix4_t model_matrix;
        vector4_t color;
    } push_k;

    bool should_do_gpu_sync = 0;
};

void ready_chunk_for_gpu_sync(voxel_chunk_t *chunk);
void initialize_chunk(voxel_chunk_t *chunk, vector3_t position);
void update_chunk_mesh(voxel_chunk_t *chunk, uint8_t surface_level);
void push_chunk_to_render_queue(voxel_chunk_t *chunk);
voxel_chunk_t **get_voxel_chunk(int32_t index);

struct voxel_chunks_t
{
    // How many chunks on the x, y and z axis
    uint32_t grid_edge_size;
    float32_t size;
    
    uint32_t chunk_count;
    uint32_t max_chunks;
    // Will be an array of pointers to the actual chunk data
    voxel_chunk_t **chunks;

    // Information that graphics pipelines need
    model_t chunk_model;

    pipeline_handle_t chunk_pipeline;

    pipeline_handle_t chunk_mesh_pipeline;
    pipeline_handle_t chunk_mesh_shadow_pipeline;

    gpu_material_submission_queue_t gpu_queue;


    uint32_t to_sync_count = 0;
    uint32_t chunks_to_gpu_sync[20];
};

using entity_handle_t = int32_t;

struct hitbox_t
{
    // Relative to the size of the entity
    // These are the values of when the entity size = 1
    float32_t x_max, x_min;
    float32_t y_max, y_min;
    float32_t z_max, z_min;
};

// Gravity acceleration on earth = 9.81 m/s^2
struct physics_component_t
{
    // Sum of mass of all particles
    float32_t mass = 1.0f; // KG
    // Sum ( all points of mass (vector quantity) * all masses of each point of mass ) / total body mass
    vector3_t center_of_gravity;
    // Depending on the shape of the body (see formula for rectangle if using hitbox, and for sphere if using bounding sphere)
    vector3_t moment_of_inertia;

    float32_t coefficient_of_restitution = 0.0f;
    
    vector3_t acceleration {0.0f};
    vector3_t ws_velocity {0.0f};
    vector3_t displacement {0.0f};

    enum is_resting_t { NOT_RESTING = 0, JUST_COLLIDED = 1, RESTING = 2 } is_resting {NOT_RESTING};

    float32_t momentum = 0.0f;
    
    // F = ma
    vector3_t total_force_on_body;
    // G = mv
    //vector3_t momentum;

    uint32_t entity_index;
    
    vector3_t gravity_accumulation = {};
    vector3_t friction_accumulation = {};
    vector3_t slide_accumulation = {};
    
    bool enabled;
    hitbox_t hitbox;
    vector3_t surface_normal;
    vector3_t surface_position;

    vector3_t force;

    // other forces (friction...)
};

struct terraform_power_component_t
{
    uint32_t entity_index;
    
    // Some sort of limit or something
    float32_t speed;
    float32_t terraform_radius;
};

struct camera_component_t
{
    uint32_t entity_index;
    
    // Can be set to -1, in that case, there is no camera bound
    camera_handle_t camera{-1};

    // Maybe some other variables to do with 3rd person / first person configs ...

    bool in_animation = false;
    quaternion_t current_rotation;

    bool is_third_person;
    float32_t distance_from_player = 40.0f;
};

struct animation_component_t
{
    uint32_t entity_index;
    // Rendering the animated entity
    animated_instance_t animation_instance;
    animation_cycles_t *cycles;
};

struct rendering_component_t
{
    uint32_t entity_index;
    
    // push constant stuff for the graphics pipeline
    struct
    {
	// in world space
	matrix4_t ws_t{1.0f};
	vector4_t color;

        float32_t roughness;
        float32_t metalness;
    } push_k;

    bool enabled = true;
};

struct entity_body_t
{
    float32_t weight = 1.0f;
    hitbox_t hitbox;
};

// Action components can be modified over keyboard / mouse input, or on a network
enum action_flags_t { ACTION_FORWARD, ACTION_LEFT, ACTION_BACK, ACTION_RIGHT, ACTION_UP, ACTION_DOWN, ACTION_RUN, ACTION_SHOOT, ACTION_TERRAFORM_DESTROY, ACTION_TERRAFORM_ADD };

struct network_component_t
{
    uint32_t entity_index;
    uint32_t client_state_index;
};

struct entity_t
{
    entity_t(void) = default;
    
    constant_string_t id {""_hash};
    // position, direction, velocity
    // in above entity group space
    vector3_t ws_p{0.0f}, ws_d{0.0f}, ws_v{0.0f}, ws_input_v{0.0f};
    vector3_t ws_acceleration {0.0f};
    quaternion_t ws_r{0.0f, 0.0f, 0.0f, 0.0f};
    vector3_t size{1.0f};

    vector3_t surface_normal;
    vector3_t surface_position;

    // Has effect on animations
    bool is_in_air = 0;
    bool is_sliding_not_rolling_mode = 0;

    bool toggled_rolling_previous_frame = 0;
    bool32_t rolling_mode;
    float32_t rolling_rotation_angle = 0.0f;
    matrix4_t rolling_rotation = matrix4_t(1.0f);

    bool is_sitting = 0;

    uint32_t action_flags = 0;
    
    //    struct entity_body_t body;
    // For animated rendering component
    enum animated_state_t { WALK, IDLE, RUN, HOVER, SLIDING_NOT_ROLLING_MODE, SITTING, JUMP } animated_state = animated_state_t::IDLE;
    
    struct components_t
    {

        int32_t camera_component;
        int32_t physics_component;
        int32_t rendering_component;
        int32_t animation_component;
        int32_t network_component;
        int32_t terraform_power_component;
        
    } components;
    
    entity_handle_t index;
};

struct dbg_entities_t
{
    bool hit_box_display = false;
    entity_t *render_sliding_vector_entity = nullptr;
};

struct entities_t
{
    dbg_entities_t dbg;

    static constexpr uint32_t MAX_ENTITIES = 30;
    
    int32_t entity_count = {};
    entity_t entity_list[MAX_ENTITIES] = {};

    // All possible components: 
    int32_t physics_component_count = {};
    struct physics_component_t physics_components[MAX_ENTITIES] = {};

    int32_t camera_component_count = {};
    struct camera_component_t camera_components[MAX_ENTITIES] = {};

    int32_t rendering_component_count = {};
    struct rendering_component_t rendering_components[MAX_ENTITIES] = {};

    int32_t animation_component_count = {};
    struct animation_component_t animation_components[MAX_ENTITIES] = {};

    int32_t network_component_count = {};
    struct network_component_t network_components[MAX_ENTITIES] = {};

    int32_t terraform_power_component_count = {};
    struct terraform_power_component_t terraform_power_components[MAX_ENTITIES] = {};

    struct hash_table_inline_t<entity_handle_t, 30, 5, 5> name_map{"map.entities"};

    pipeline_handle_t entity_ppln;
    pipeline_handle_t entity_shadow_ppln;

    pipeline_handle_t rolling_entity_ppln;
    pipeline_handle_t rolling_entity_shadow_ppln;
    
    pipeline_handle_t dbg_hitbox_ppln;

    mesh_t rolling_entity_mesh;
    model_t rolling_entity_model;

    mesh_t entity_mesh;
    skeleton_t entity_mesh_skeleton;
    animation_cycles_t entity_mesh_cycles;
    uniform_layout_t animation_ubo_layout;
    model_t entity_model;

    // For now:
    int32_t main_entity = -1;
    // have some sort of stack of REMOVED entities

    gpu_material_submission_queue_t entity_submission_queue;
    gpu_material_submission_queue_t rolling_entity_submission_queue;
};

uint32_t add_network_component(void);
struct network_component_t *get_network_component(uint32_t index);

struct world_t
{
    struct entities_t entities;
    struct voxel_chunks_t voxel_chunks;
};

enum entity_color_t { BLUE, RED, GRAY, DARK_GRAY, GREEN, INVALID_COLOR };

entity_t *get_entity(entity_handle_t entity_handle);
voxel_chunk_t **get_voxel_chunk(uint32_t index);

// For now, take in the color
entity_handle_t spawn_entity(const char *entity_name, entity_color_t color);
entity_handle_t spawn_entity_at(const char *entity_name, entity_color_t color, const vector3_t &ws_position, const quaternion_t &quat);
void make_entity_renderable(entity_handle_t entity_handle, entity_color_t color);
void make_entity_main(entity_handle_t entity_handle, input_state_t *input_state);

void update_network_world_state(void);

void clean_up_world_data(void);

void make_world_data(void);

void set_focus_for_world(void);
void remove_focus_for_world(void);

void hard_initialize_world(input_state_t *input_state, VkCommandPool *cmdpool, enum application_type_t app_type, enum application_mode_t app_mode);
void initialize_world(input_state_t *input_state, VkCommandPool *cmdpool, enum application_type_t type, enum application_mode_t mode);

void update_world(input_state_t *input_state, float32_t dt, uint32_t image_index,
                  uint32_t current_frame, gpu_command_queue_t *queue, enum application_type_t type, enum element_focus_t focus);

void handle_world_input(input_state_t *input_state, float32_t dt);

void handle_input_debug(input_state_t *input_state, float32_t dt);

void destroy_world(void);

void initialize_world_translation_unit(struct game_memory_t *memory);
