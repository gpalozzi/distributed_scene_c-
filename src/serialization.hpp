#ifndef _SERIALIZATION_HPP_
#define _SERIALIZATION_HPP_

namespace boost {
    namespace serialization {
        
        // vec3f serialization
        template<class Archive>
        void serialize(Archive & ar, vec3f& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
            ar & v.z;
        }
        
        // vec2f serialization
        template<class Archive>
        void serialize(Archive & ar, vec2f& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
        }
        
        // vec4i serialization
        template<class Archive>
        void serialize(Archive & ar, vec4i& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
            ar & v.z;
            ar & v.w;
        }

        // vec3i serialization
        template<class Archive>
        void serialize(Archive & ar, vec3i& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
            ar & v.z;
        }
        
        // vec2i serialization
        template<class Archive>
        void serialize(Archive & ar, vec2i& v, const unsigned int version)
        {
            ar & v.x;
            ar & v.y;
        }

        // vec4id serialization
        template<class Archive>
        void serialize(Archive & ar, vec4id& v, const unsigned int version)
        {
            ar & v.first;
            ar & v.second;
            ar & v.third;
            ar & v.fourth;
        }

        // vec3id serialization
        template<class Archive>
        void serialize(Archive & ar, vec3id& v, const unsigned int version)
        {
            ar & v.first;
            ar & v.second;
            ar & v.third;
        }
        
        // vec2id serialization
        template<class Archive>
        void serialize(Archive & ar, vec2id& v, const unsigned int version)
        {
            ar & v.first;
            ar & v.second;
        }

        // frame3f serialization
        template<class Archive>
        void serialize(Archive & ar, frame3f& f, const unsigned int version)
        {
            ar & f.o;
            ar & f.x;
            ar & f.y;
            ar & f.z;
        }

        // camera serialization
        template<class Archive>
        void serialize(Archive & ar, Camera& c, const unsigned int version)
        {
            ar & c.frame;
            ar & c.width;
            ar & c.height;
            ar & c.dist;
            ar & c.focus;
            ar & c._id_;
        }
        
        // light serialization
        template<class Archive>
        void serialize(Archive & ar, Light& l, const unsigned int version)
        {
            ar & l.frame;
            ar & l.intensity;
            ar & l._id_;
        }
        
        // material serialization
        template<class Archive>
        void serialize(Archive & ar, Material& m, const unsigned int version)
        {
            ar & m.ke;
            ar & m.kd;
            ar & m.ks;
            ar & m.n;
            ar & m.kr;
            ar & m._id_;
        }
        
        // mesh serialization
        template<class Archive>
        void serialize(Archive & ar, Mesh& m, const unsigned int version)
        {
            ar & m.frame;
            ar & m.vertex_ids;
            ar & m.pos;
            ar & m.normal_ids;
            ar & m.norm;
            ar & m.texcoord_ids;
            ar & m.texcoord;
            ar & m.triangle_ids;
            ar & m.triangle;
            ar & m.triangle_index;
            ar & m.quad_ids;
            ar & m.quad;
            ar & m.quad_index;
            ar & m.edge_ids;
            ar & m.edge;
            ar & m.edge_index;
            ar & m.point;
            ar & m.line;
            ar & m.spline;
            ar & m.vertex_id_map;
            ar & m.mat;
            ar & m._id_;
        }
        
        // scene serialization
        template<class Archive>
        void serialize(Archive & ar, Scene& s, const unsigned int version)
        {
            ar & s.camera;
            ar & s.meshes;
            //ar & s.surfaces;
            ar & s.lights;
            ar & s.materials;
            ar & s.background;
            ar & s.ambient;
            ar & s.image_width;
            ar & s.image_height;
            ar & s.image_samples;
            ar & s.ids_map;

        }
        
    } // namespace serialization
} // namespace boost


#endif
