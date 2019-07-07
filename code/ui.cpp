#include "graphics.hpp"

// Start from bottom left
enum Coordinate_Type { PIXEL, GLSL };

struct UI_V2
{
    union
    {
        struct {s32 ix, iy;};
        struct {f32 fx, fy;};
    };
    Coordinate_Type type;

    UI_V2(void) = default;
    UI_V2(f32 x, f32 y) : fx(x), fy(y), type(GLSL) {}
    UI_V2(s32 x, s32 y) : ix(x), iy(y), type(PIXEL) {}

    glm::vec2
    to_fvec2(void) const
    {
        return glm::vec2(fx, fy);
    }

    glm::ivec2
    to_ivec2(void) const
    {
        return glm::ivec2(ix, iy);
    }
};

using UI_GLSL_V2 = UI_V2;
using UI_PX_V2 = UI_V2;


struct Fonts
{
    struct Font_Vertex
    {
        glm::vec2 position;
        glm::vec2 uvs;
    };

    persist constexpr u32 FONT_VBO_SIZE = 65536;
    persist constexpr u32 MAX_FONT_VERTICES_PER_UPDATE = FONT_VBO_SIZE / sizeof(Font_Vertex);
    persist constexpr u32 MAX_FONT_QUADS_PER_UPDATE = MAX_FONT_VERTICES_PER_UPDATE / 6;
    
    persist Font_Vertex cpu_vertex_pool[ MAX_FONT_VERTICES_PER_UPDATE ];
    
    Model_Handle font_quads_model;
    GPU_Buffer_Handle font_vbo;
    // Is compatible with post processing render pass
    Pipeline_Handle font_ppln;
} g_fonts;

internal UI_PX_V2
glsl_to_pixel_coord(const UI_GLSL_V2 &position,
                    const Resolution &resolution)
{
    UI_PX_V2 ret((s32)((u32)position.fx * resolution.width), (s32)((u32)position.fy * resolution.height));
    return(ret);
}

internal UI_GLSL_V2
pixel_to_glsl_coord(const UI_PX_V2 &position,
                    const Resolution &resolution)
{
    UI_GLSL_V2 ret((f32)position.ix / (f32)resolution.width
                   , (f32)position.iy / (f32)resolution.height);
    return(ret);
}

// For now, doesn't support the relative to function
enum Relative_To { LEFT_UP, LEFT_DOWN, CENTER, RIGHT_UP, RIGHT_DOWN };

struct UI_Box
{
    UI_Box *parent {nullptr};
    
    Relative_To relative_to;
    UI_V2 position;
    
    f32 aspect_ratio;
    UI_GLSL_V2 gls_max_values;
    
    UI_PX_V2 px_current_size;
    UI_GLSL_V2 gls_current_size;

    void
    update_size(Resolution backbuffer_res)
    {
        UI_PX_V2 px_max_values = glsl_to_pixel_coord(this->gls_max_values, backbuffer_res);
        UI_PX_V2 px_max_xvalue_coord(px_max_values.ix, (s32)((f32)px_max_values.ix / this->aspect_ratio));
        // Check if, using glsl max x value, the y still fits in the given glsl max y value
        if (px_max_xvalue_coord.iy <= px_max_values.iy)
        {
            // Then use this new size;
            this->px_current_size = px_max_xvalue_coord;
        }
        else
        {
            // Then use y max value, and modify the x dependending on the new y
            UI_PX_V2 px_max_yvalue_coord((u32)((f32)px_max_values.iy * aspect_ratio), px_max_values.iy);
            this->px_current_size = px_max_yvalue_coord;
        }
        gls_current_size = pixel_to_glsl_coord(this->px_current_size, backbuffer_res);
    }

    UI_GLSL_V2
    get_glsl_position(Resolution backbuffer_res)
    {
        if (this->position.type == GLSL) return (UI_GLSL_V2)this->position;
        else return pixel_to_glsl_coord((const UI_PX_V2 &)this->position, backbuffer_res);
    }

    UI_GLSL_V2
    get_px_position(Resolution backbuffer_res)
    {
        if (this->position.type == PIXEL) return (UI_PX_V2)this->position;
        else return glsl_to_pixel_coord((const UI_GLSL_V2 &)this->position, backbuffer_res);
    }
};

internal UI_Box
make_ui_box(Relative_To relative_to, f32 aspect_ratio,
            UI_V2 position /* Coord space agnostic */,
            UI_GLSL_V2 gls_max_values /* Max X and Y size */,
            UI_Box *parent,
            Resolution backbuffer_res = {})
{
    UI_Box box = {};
    box.parent = parent;
    box.aspect_ratio = aspect_ratio;
    box.gls_max_values = gls_max_values;
    // Updates current size value
    box.update_size(backbuffer_res);

    box.relative_to = relative_to;
    box.position = position;
}

// Will be rendered to backbuffer first
void
initialize_ui(Vulkan::GPU *gpu, Resolution resolution)
{
    g_fonts.font_quads_model = g_model_manager.add("model.font_quads"_hash);
    auto *font_quads_ptr = g_model_manager.get(g_fonts.font_quads_model);
    {
        font_quads_ptr->attribute_count = 2;
	font_quads_ptr->attributes_buffer = (VkVertexInputAttributeDescription *)allocate_free_list(sizeof(VkVertexInputAttributeDescription) * 3);
	font_quads_ptr->binding_count = 1;
	font_quads_ptr->bindings = (Vulkan::Model_Binding *)allocate_free_list(sizeof(Vulkan::Model_Binding));

	// only one binding
	Vulkan::Model_Binding *binding = font_quads_ptr->bindings;
	binding->begin_attributes_creation(font_quads_ptr->attributes_buffer);

	binding->push_attribute(0, VK_FORMAT_R32G32_SFLOAT, sizeof(Fonts::Font_Vertex::position));
	binding->push_attribute(1, VK_FORMAT_R32G32_SFLOAT, sizeof(Fonts::Font_Vertex::uvs));

	binding->end_attributes_creation();
    }

    g_fonts.font_vbo = g_gpu_buffer_manager.add("vbo.font_quads"_hash);
    auto *vbo = g_gpu_buffer_manager.get(g_fonts.font_vbo);
    {
        auto *main_binding = &font_quads_ptr->bindings[0];
	
        Vulkan::init_buffer(Fonts::FONT_VBO_SIZE,
                            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            VK_SHARING_MODE_EXCLUSIVE,
                            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                            gpu,
                            vbo);

        main_binding->buffer = vbo->buffer;
        font_quads_ptr->create_vbo_list();
    }    
}
