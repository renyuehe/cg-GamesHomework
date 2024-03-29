#include <iostream>
#include <opencv2/opencv.hpp>

#include "global.hpp"
#include "rasterizer.hpp"
#include "Triangle.hpp"
#include "Shader.hpp"
#include "Texture.hpp"
#include "OBJ_Loader.h"

/******************* mvp ********************/
Eigen::Matrix4f get_view_matrix(Eigen::Vector3f eye_pos)
{
    Eigen::Matrix4f view = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f translate;
    translate << 1,0,0,-eye_pos[0],
                 0,1,0,-eye_pos[1],
                 0,0,1,-eye_pos[2],
                 0,0,0,1;

    view = translate*view;

    return view;
}

Eigen::Matrix4f get_model_matrix(float angle)
{
    Eigen::Matrix4f rotation;
    angle = angle * MY_PI / 180.f;
    rotation << cos(angle), 0, sin(angle), 0,
                0, 1, 0, 0,
                -sin(angle), 0, cos(angle), 0,
                0, 0, 0, 1;

    Eigen::Matrix4f scale;
    scale << 2.5, 0, 0, 0,
              0, 2.5, 0, 0,
              0, 0, 2.5, 0,
              0, 0, 0, 1;

    Eigen::Matrix4f translate;
    translate << 1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 1;

    return translate * rotation * scale;
}

Eigen::Matrix4f get_projection_matrix(float eye_fov, float aspect_ratio, float zNear, float zFar)
{
    // TODO: Use the same projection matrix from the previous assignments

    Eigen::Matrix4f projection = Eigen::Matrix4f::Identity();

    Eigen::Matrix4f pto = Eigen::Matrix4f::Identity();
    pto << zNear,0,0,0,
            0,zNear,0,0,
            0,0,(zNear+zFar),(-1*zFar*zNear),
            0,0,1,0;

    //float halfAngle = eye_fov/2.0 * MY_PI /180.0f;
    //float top = -1.0f * tan(halfAngle) * zNear;
    //float bottom = -1.0f * top;
    //float right = top * aspect_ratio;
    //float left = -1.0f * right;

    float halfAngle = eye_fov * MY_PI / 180.0f;
    float height = zNear * tan(halfAngle) * 2;
    float width = height * aspect_ratio;

    auto top = -zNear * tan(halfAngle / 2);
    auto right = top * aspect_ratio;
    auto left = -right;
    auto bottom = -top;


    Eigen::Matrix4f m_s = Eigen::Matrix4f::Identity();
    m_s << 2/(right-left),0,0,0,
            0,2/(top-bottom),0,0,
            0,0,2/(zNear-zFar),0,
            0,0,0,1;

    Eigen::Matrix4f m_t = Eigen::Matrix4f::Identity();
    m_t << 1,0,0,-(right+left)/2,
            0,1,0,-(top + bottom)/2,
            0,0,1,-(zFar+zNear)/2,
            0,0,0,1;

    projection = m_s * m_t * pto * projection;
    return projection;

}


/************************ shader ************************/
Eigen::Vector3f vertex_shader(const vertex_shader_payload& payload)
{
    return payload.position;
}

Eigen::Vector3f normal_fragment_shader(const fragment_shader_payload& payload)
{
    // Eigen::Vector3f return_color = (payload.normal.head<3>().normalized() + Eigen::Vector3f(1.0f, 1.0f, 1.0f)) / 2.f;
    Eigen::Vector3f return_color = payload.normal.head<3>().normalized();
    Eigen::Vector3f result;
    result << return_color.x() * 255, return_color.y() * 255, return_color.z() * 255;
    return result;
}

static Eigen::Vector3f reflect(const Eigen::Vector3f& vec, const Eigen::Vector3f& axis)
{
    auto costheta = vec.dot(axis);
    return (2 * costheta * axis - vec).normalized();
}

struct light
{
    Eigen::Vector3f position;
    Eigen::Vector3f intensity;//强度
};

Eigen::Vector3f texture_fragment_shader(const fragment_shader_payload& payload)
{
    Eigen::Vector3f texture_color = {0, 0, 0};
    if (payload.texture)
    {
        // TODO: Get the texture value at the texture coordinates of the current fragment
        //当前点对应的uv坐标已经在光栅化的过程中计算好放在payload中了，用uv坐标在texture中获取对应颜色就可以
        //与phong_fragment_shader()函数的主要差别在这，kd的取值不一样。
        texture_color = payload.texture->getColor(payload.tex_coords.x(),payload.tex_coords.y());
    }

    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = texture_color / 255.f;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = texture_color;
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;

    Eigen::Vector3f result_color = {0, 0, 0};

    for (auto& light : lights)
    {
        // TODO: For each light source in the code, calculate what the *ambient*, *diffuse*, and *specular* 
        // components are. Then, accumulate that result on the *result_color* object.

        //光的方向
        Eigen::Vector3f light_dir = light.position - point;
        //视线方向
        Eigen::Vector3f view_dir = eye_pos - point;

        //衰减因子r,两个向量的点乘
        float r2 = light_dir.dot(light_dir);
        
        //ambient
        Eigen::Vector3f La = ka.cwiseProduct(amb_light_intensity);

        //diffuse
        //实现材质贴图，只需要将漫反射项的系数改为材质贴图的对应值即可
        Eigen::Vector3f Ld = kd.cwiseProduct(light.intensity / r2);   
        Ld *= std::max(0.0f,normal.normalized().dot(light_dir.normalized()));

        //specular
        Eigen::Vector3f h = (light_dir + view_dir).normalized();
        Eigen::Vector3f Ls = ks.cwiseProduct(light.intensity / r2);
        Ls *= std::pow(std::max(0.0f,normal.normalized().dot(h)),p);

        result_color += (La + Ld + Ls);

    }

    return result_color * 255.f;
}

Eigen::Vector3f phong_fragment_shader(const fragment_shader_payload& payload)
{
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);//环境光系数
    Eigen::Vector3f kd = payload.color;//漫反射系数
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);//镜面反射系数

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};//光源
    Eigen::Vector3f amb_light_intensity{10, 10, 10};//环境光
    Eigen::Vector3f eye_pos{0, 0, 10};//眼睛位置

    float p = 150;//高光范围控制：指数控制系数

    Eigen::Vector3f color = payload.color;
    Eigen::Vector3f point = payload.view_pos;//视图坐标
    Eigen::Vector3f normal = payload.normal;

    Eigen::Vector3f result_color = {0, 0, 0};
    for (auto& light : lights)
    {
        // TODO: For each light source in the code, calculate what the *ambient*, *diffuse*, and *specular* 
        // components are. Then, accumulate that result on the *result_color* object.

        //光的方向
        Eigen::Vector3f light_dir = light.position - point;
        //视线方向
        Eigen::Vector3f view_dir = eye_pos - point;
        //衰减因子r,两个向量的点乘
        float r2 = light_dir.dot(light_dir);
        
        //ambient环境光照
        Eigen::Vector3f La = ka.cwiseProduct(amb_light_intensity);//cwiseProduct：一对一相乘，结果依然是向量或矩阵
        
        //diffuse漫反射
        Eigen::Vector3f Ld = kd.cwiseProduct(light.intensity / r2);//光强度衰减项
        Ld *= std::max(0.0f,normal.normalized().dot(light_dir.normalized()));
        
        //specular镜面反射
        //半程向量：视线方向和光线方向平均
        Eigen::Vector3f h = (light_dir + view_dir).normalized();//半程向量
        Eigen::Vector3f Ls = ks.cwiseProduct(light.intensity / r2);//光强度衰减项
        Ls *= std::pow(std::max(0.0f,normal.normalized().dot(h)),p);
        
		//三者相加
        result_color += (La + Ld + Ls);
    }

    return result_color * 255.f;
}

Eigen::Vector3f displacement_fragment_shader(const fragment_shader_payload& payload)
{
    
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = payload.color;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};
    Eigen::Vector3f amb_light_intensity{10, 10, 10};
    Eigen::Vector3f eye_pos{0, 0, 10};

    float p = 150;

    Eigen::Vector3f color = payload.color; 
    Eigen::Vector3f point = payload.view_pos;
    Eigen::Vector3f normal = payload.normal;

    float kh = 0.2, kn = 0.1;
    
    // TODO: Implement displacement mapping here
    // Let n = normal = (x, y, z)
    // Vector t = (x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z))
    // Vector b = n cross product t
    // Matrix TBN = [t b n]
    // dU = kh * kn * (h(u+1/w,v)-h(u,v))
    // dV = kh * kn * (h(u,v+1/h)-h(u,v))
    // Vector ln = (-dU, -dV, 1)
    // Position p = p + kn * n * h(u,v)
    // Normal n = normalize(TBN * ln)


    Eigen::Vector3f result_color = {0, 0, 0};

    for (auto& light : lights)
    {
        // TODO: For each light source in the code, calculate what the *ambient*, *diffuse*, and *specular* 
        // components are. Then, accumulate that result on the *result_color* object.


    }

    return result_color * 255.f;
}

Eigen::Vector3f bump_fragment_shader(const fragment_shader_payload& payload)
{
    
    Eigen::Vector3f ka = Eigen::Vector3f(0.005, 0.005, 0.005);
    Eigen::Vector3f kd = payload.color;
    Eigen::Vector3f ks = Eigen::Vector3f(0.7937, 0.7937, 0.7937);

    auto l1 = light{{20, 20, 20}, {500, 500, 500}};
    auto l2 = light{{-20, 20, 0}, {500, 500, 500}};

    std::vector<light> lights = {l1, l2};//点光源
    Eigen::Vector3f amb_light_intensity{10, 10, 10};//环境光
    Eigen::Vector3f eye_pos{0, 0, 10};//摄像机位置

    float p = 150;//指数：高光项大小

    Eigen::Vector3f color = payload.color;
    Eigen::Vector3f point = payload.view_pos;//视图坐标
    Eigen::Vector3f normal = payload.normal;

    float kh = 0.2, kn = 0.1;

    // TODO: Implement bump mapping here
    // Let n = normal = (x, y, z)
    // Vector t = (x*y/sqrt(x*x+z*z),sqrt(x*x+z*z),z*y/sqrt(x*x+z*z))
    // Vector b = n cross product t
    // Matrix TBN = [t b n]
    // dU = kh * kn * (h(u+1/w,v)-h(u,v))
    // dV = kh * kn * (h(u,v+1/h)-h(u,v))
    // Vector ln = (-dU, -dV, 1)
    // Normal n = normalize(TBN * ln)

    Eigen::Vector3f result_color = normal;
    return result_color * 255.f;
}

int main(int argc, const char** argv)
{
    std::vector<Triangle*> TriangleList;

    float angle = 140.0;
    bool command_line = false;

    //输出
    std::string filename = "output.png";
    
    //obj
    objl::Loader Loader;
    std::string obj_path = "../models/spot/";
    bool loadout = Loader.LoadFile("../models/spot/spot_triangulated_good.obj");

    for(auto mesh:Loader.LoadedMeshes)
    {
        //遍历三角形
        for(int i=0;i<mesh.Vertices.size();i+=3)
        {
            Triangle* t = new Triangle();
            for(int j=0;j<3;j++)
            {
                t->setVertex(j,Vector4f(mesh.Vertices[i+j].Position.X,mesh.Vertices[i+j].Position.Y,mesh.Vertices[i+j].Position.Z,1.0));
                t->setNormal(j,Vector3f(mesh.Vertices[i+j].Normal.X,mesh.Vertices[i+j].Normal.Y,mesh.Vertices[i+j].Normal.Z));
                t->setTexCoord(j,Vector2f(mesh.Vertices[i+j].TextureCoordinate.X, mesh.Vertices[i+j].TextureCoordinate.Y));
            }
            TriangleList.push_back(t);
        }
    }

    rst::rasterizer r(700, 700);

    //纹理贴图
    auto texture_path = "hmap.jpg";
    r.set_texture(Texture(obj_path + texture_path));
    //着色 函数
    std::function<Eigen::Vector3f(fragment_shader_payload)> active_shader = phong_fragment_shader;
    if (argc >= 2)
    {
        command_line = true;
        filename = std::string(argv[1]);

        if (argc == 3 && std::string(argv[2]) == "texture")
        {
            std::cout << "Rasterizing using the texture shader\n";
            active_shader = texture_fragment_shader;
            texture_path = "spot_texture.png";
            r.set_texture(Texture(obj_path + texture_path));
        }
        else if (argc == 3 && std::string(argv[2]) == "normal")
        {
            std::cout << "Rasterizing using the normal shader\n";
            active_shader = normal_fragment_shader;
        }
        else if (argc == 3 && std::string(argv[2]) == "phong")
        {
            std::cout << "Rasterizing using the phong shader\n";
            active_shader = phong_fragment_shader;
        }
        else if (argc == 3 && std::string(argv[2]) == "bump")
        {
            std::cout << "Rasterizing using the bump shader\n";
            active_shader = bump_fragment_shader;
        }
        else if (argc == 3 && std::string(argv[2]) == "displacement")
        {
            std::cout << "Rasterizing using the bump shader\n";
            active_shader = displacement_fragment_shader;
        }
        else
        {
            std::cout << " no equal " << std::endl;
            return 0;
        }
    }


    //摄像机位置
    Eigen::Vector3f eye_pos = {0,0,10};

    //顶点着色器，比如 Gouraud Shader
    r.set_vertex_shader(vertex_shader);
    //fragment（相当于像素）着色器，Flat，Phong，Texture，bump
    r.set_fragment_shader(active_shader);

    int key = 0;
    int frame_count = 0;

    if (command_line)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

        r.draw(TriangleList);
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

        cv::imwrite(filename, image);

        return 0;
    }

    while(key != 27)
    {
        r.clear(rst::Buffers::Color | rst::Buffers::Depth);

        //mvp
        r.set_model(get_model_matrix(angle));
        r.set_view(get_view_matrix(eye_pos));
        r.set_projection(get_projection_matrix(45.0, 1, 0.1, 50));

        //r.draw(pos_id, ind_id, col_id, rst::Primitive::Triangle);
        //绘制（光栅化）
        r.draw(TriangleList);

        //转换成图像格式
        cv::Mat image(700, 700, CV_32FC3, r.frame_buffer().data());
        image.convertTo(image, CV_8UC3, 1.0f);
        cv::cvtColor(image, image, cv::COLOR_RGB2BGR);

        cv::imshow("image", image);
        cv::imwrite(filename, image);
        key = cv::waitKey(10);

        if (key == 'a' )
        {
            angle -= 0.1;
        }
        else if (key == 'd')
        {
            angle += 0.1;
        }
    }
    return 0;
}
