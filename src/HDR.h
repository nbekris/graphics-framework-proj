#include <string>
#include "texture.h"

class HDR : public Texture
{
	public:
		//unsigned int textureId;
		//int width, height, depth;
		float* image;
		HDR();
		HDR(const std::string& filePath, bool keepPixels = false);
		void FreePixels();
		void BindTexture(const int unit, const int programId, const std::string& name);
		void UnbindTexture(const int unit);
};
