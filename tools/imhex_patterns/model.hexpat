struct Vertex {
    s16 pos[3];
    s16 pad1;
    
    u8 uv[2];
    u16 pad2;
    
    u8 color[3];
    u8 pad3;
};

struct Tri {
    Vertex vertices[3];
};


struct Quad {
    Vertex vertices[4];
};

struct Submesh {
    u16 subdivide;
    u16 untextured_tri_num;
    u16 untextured_quad_num;
    u16 tri_num;
    u16 quad_num;
    
    Tri untextured_tris[untextured_tri_num];
    Quad untextured_quads[untextured_quad_num];
    Tri tris[tri_num];
    Quad quads[quad_num];
};

struct Model {
    u16 submeshes_size;
    Submesh submeshes[submeshes_size];
};

Model model @ 0x0;