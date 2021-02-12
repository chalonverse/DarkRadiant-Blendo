#include "WavefrontExporter.h"

#include "itextstream.h"
#include "imodelsurface.h"
#include "imap.h"
#include "ishaders.h"

namespace model
{

namespace
{
    const char* const EXPORT_COMMENT_HEADER = "# Generated by DarkRadiant's OBJ file exporter";
}

IModelExporterPtr WavefrontExporter::clone()
{
	return std::make_shared<WavefrontExporter>();
}

const std::string& WavefrontExporter::getDisplayName() const
{
	static std::string _extension("Wavefront OBJ");
	return _extension;
}

const std::string& WavefrontExporter::getExtension() const
{
	static std::string _extension("OBJ");
	return _extension;
}

void WavefrontExporter::exportToPath(const std::string& outputPath, const std::string& filename)
{
    // Open the stream to the .obj file (and the .mtl file)
    stream::ExportStream objFile(outputPath, filename, stream::ExportStream::Mode::Text);

    fs::path mtlFilename(filename);
    mtlFilename.replace_extension(".mtl");
    stream::ExportStream mtlFile(outputPath, mtlFilename.string(), stream::ExportStream::Mode::Text);

    writeObjFile(objFile.getStream(), mtlFilename.string());
    writeMaterialLib(mtlFile.getStream());

    objFile.close();
    mtlFile.close();
}

void WavefrontExporter::writeObjFile(std::ostream& stream, const std::string& mtlFilename)
{
    // Write export comment
    stream << EXPORT_COMMENT_HEADER << std::endl;

    // Write mtllib file
    stream << "mtllib " << mtlFilename << std::endl;
    stream << std::endl;

	// Count exported vertices. Exported indices are 1-based though.
	std::size_t vertexCount = 0;

	// Each surface is exported as group.
	for (const auto& pair : _surfaces)
	{
		const Surface& surface = pair.second;

		// Base index for vertices, added to the surface indices
		std::size_t vertBaseIndex = vertexCount;

		// Store the material into the group name
		stream << "g " << surface.materialName << std::endl;

        // Reference the material we're going to export to the .mtl file
        stream << "usemtl " << surface.materialName << std::endl;
		stream << std::endl;

		// Temporary buffers for vertices, texcoords and polys
		std::stringstream vertexBuf;
		std::stringstream texCoordBuf;
		std::stringstream polyBuf;

		for (const ArbitraryMeshVertex& meshVertex : surface.vertices)
		{
			// Write coordinates into the export buffers
			const Vector3& vert = meshVertex.vertex;
			const Vector2& uv = meshVertex.texcoord;

			vertexBuf << "v " << vert.x() << " " << vert.y() << " " << vert.z() << "\n";
			texCoordBuf << "vt " << uv.x() << " " << -uv.y() << "\n"; // invert the V coordinate

			vertexCount++;
		}

		// Every three indices form a triangle. Indices are 1-based so add +1 to each index
		for (std::size_t i = 0; i + 2 < surface.indices.size(); i += 3)
		{
			std::size_t index1 = vertBaseIndex + static_cast<std::size_t>(surface.indices[i+0]) + 1;
			std::size_t index2 = vertBaseIndex + static_cast<std::size_t>(surface.indices[i+1]) + 1;
			std::size_t index3 = vertBaseIndex + static_cast<std::size_t>(surface.indices[i+2]) + 1;

			// f 1/1 3/3 2/2
			polyBuf << "f";
			polyBuf << " " << index1 << "/" << index1;
			polyBuf << " " << index2 << "/" << index2;
			polyBuf << " " << index3 << "/" << index3;
			polyBuf << "\n";
		}

		stream << vertexBuf.str() << std::endl;
		stream << texCoordBuf.str() << std::endl;
		stream << polyBuf.str() << std::endl;
	}
}

void WavefrontExporter::writeMaterialLib(std::ostream& stream)
{
    // Write export comment
    stream << EXPORT_COMMENT_HEADER << std::endl;

    for (const auto& pair : _surfaces)
    {
        const Surface& surface = pair.second;

        auto material = GlobalMaterialManager().getMaterialForName(surface.materialName);

        const auto layers = material->getAllLayers();

        stream << "newmtl " << surface.materialName << std::endl;
        stream << "Ns 0.0" << std::endl; // shininess
        stream << "Ka 1.000000 1.000000 1.000000" << std::endl; // ambient colour
        stream << "Kd 1.000000 1.000000 1.000000" << std::endl; // diffuse colour
        stream << "Ks 1.000000 1.000000 1.000000" << std::endl; // specular colour
        stream << "d 1.000000" << std::endl; // (not transparent at all)

        std::string diffuseFilename;
        std::string specularFilename;
        std::string bumpFilename;

        for (const auto& layer : layers)
        {
            if (layer->getType() == ShaderLayer::DIFFUSE)
            {
                diffuseFilename = layer->getMapImageFilename();
            }
            else if (layer->getType() == ShaderLayer::BUMP)
            {
                bumpFilename = layer->getMapImageFilename();
            }
            else if (layer->getType() == ShaderLayer::SPECULAR)
            {
                specularFilename = layer->getMapImageFilename();
            }
        }

        if (!diffuseFilename.empty())
        {
            stream << "map_Kd " << diffuseFilename << std::endl; // diffusemap
        }

        if (!bumpFilename.empty())
        {
            stream << "map_Kn " << bumpFilename << std::endl; // normalmap
        }

        if (!specularFilename.empty())
        {
            stream << "map_Ks " << specularFilename << std::endl; // specularmap
            stream << "illum 2" << std::endl; // specular colour
        }
        else
        {
            stream << "illum 1" << std::endl; // specular colour
        }

        stream << std::endl;
        stream << std::endl;
    }
}

}
