// Copyright (c) 2012- PPSSPP Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0 or later versions.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#include <list>
#include <set>
#include <algorithm>

#include "gfx/gl_common.h"
#include "gfx_es2/fbo.h"
// Keeps track of allocated FBOs.
// Also provides facilities for drawing and later converting raw
// pixel data.


#include "../Globals.h"
#include "GPU/GPUCommon.h"
#include "GPU/Common/FramebufferCommon.h"

struct GLSLProgram;
class TextureCache;
class TransformDrawEngine;
class ShaderManager;

void CenterRect(float *x, float *y, float *w, float *h,
								float origW, float origH, float frameW, float frameH);

#ifndef USING_GLES2
// Simple struct for asynchronous PBO readbacks
struct AsyncPBO {
	GLuint handle;
	u32 maxSize;

	u32 fb_address;
	u32 stride;
	u32 height;
	u32 size;
	GEBufferFormat format;
	bool reading;
};

#endif

class FramebufferManager : public FramebufferManagerCommon {
public:
	FramebufferManager();
	~FramebufferManager();

	void SetTextureCache(TextureCache *tc) {
		textureCache_ = tc;
	}
	void SetShaderManager(ShaderManager *sm) {
		shaderManager_ = sm;
	}
	void SetTransformDrawEngine(TransformDrawEngine *td) {
		transformDraw_ = td;
	}

	void MakePixelTexture(const u8 *srcPixels, GEBufferFormat srcPixelFormat, int srcStride, int width, int height);

	void DrawPixels(VirtualFramebuffer *vfb, int dstX, int dstY, const u8 *srcPixels, GEBufferFormat srcPixelFormat, int srcStride, int width, int height);
	void DrawFramebuffer(const u8 *srcPixels, GEBufferFormat srcPixelFormat, int srcStride, bool applyPostShader);

	// If texture != 0, will bind it.
	// x,y,w,h are relative to destW, destH which fill out the target completely.
	void DrawActiveTexture(GLuint texture, float x, float y, float w, float h, float destW, float destH, bool flip = false, float u0 = 0.0f, float v0 = 0.0f, float u1 = 1.0f, float v1 = 1.0f, GLSLProgram *program = 0);

	void DrawPlainColor(u32 color);

	void DestroyAllFBOs();

	void Init();
	void EndFrame();
	void Resized();
	void DeviceLost();
	void CopyDisplayToOutput();
	void UpdateFromMemory(u32 addr, int size, bool safe);
	void SetLineWidth();
	void ReformatFramebufferFrom(VirtualFramebuffer *vfb, GEBufferFormat old);

	void BlitFramebufferDepth(VirtualFramebuffer *sourceframebuffer, VirtualFramebuffer *targetframebuffer);

	// For use when texturing from a framebuffer.  May create a duplicate if target.
	void BindFramebufferColor(VirtualFramebuffer *framebuffer, bool skipCopy = false);

	// Returns true if it's sure this is a direct FBO->FBO transfer and it has already handle it.
	// In that case we hardly need to actually copy the bytes in VRAM, they will be wrong anyway (unless
	// read framebuffers is on, in which case this should always return false).
	bool NotifyBlockTransferBefore(u32 dstBasePtr, int dstStride, int dstX, int dstY, u32 srcBasePtr, int srcStride, int srcX, int srcY, int w, int h, int bpp);
	void NotifyBlockTransferAfter(u32 dstBasePtr, int dstStride, int dstX, int dstY, u32 srcBasePtr, int srcStride, int srcX, int srcY, int w, int h, int bpp);

	// Reads a rectangular subregion of a framebuffer to the right position in its backing memory.
	void ReadFramebufferToMemory(VirtualFramebuffer *vfb, bool sync, int x, int y, int w, int h);

	std::vector<FramebufferInfo> GetFramebufferList();

	bool NotifyFramebufferCopy(u32 src, u32 dest, int size, bool isMemset = false);
	bool NotifyStencilUpload(u32 addr, int size, bool skipZero = false);

	void DestroyFramebuf(VirtualFramebuffer *vfb);
	void ResizeFramebufFBO(VirtualFramebuffer *vfb, u16 w, u16 h, bool force = false);

	bool GetCurrentFramebuffer(GPUDebugBuffer &buffer);
	bool GetCurrentDepthbuffer(GPUDebugBuffer &buffer);
	bool GetCurrentStencilbuffer(GPUDebugBuffer &buffer);

	void RebindFramebuffer();

	FBO *GetTempFBO(u16 w, u16 h, FBOColorDepth depth = FBO_8888);

protected:
	virtual void DisableState() override;
	virtual void ClearBuffer() override;
	virtual void ClearDepthBuffer() override;
	virtual void FlushBeforeCopy() override;
	virtual void DecimateFBOs() override;

	virtual void NotifyRenderFramebufferCreated(VirtualFramebuffer *vfb) override;
	virtual void NotifyRenderFramebufferSwitched(VirtualFramebuffer *prevVfb, VirtualFramebuffer *vfb) override;
	virtual void NotifyRenderFramebufferUpdated(VirtualFramebuffer *vfb, bool vfbFormatChanged) override;

private:
	void CompileDraw2DProgram();
	void DestroyDraw2DProgram();

	void FindTransferFramebuffers(VirtualFramebuffer *&dstBuffer, VirtualFramebuffer *&srcBuffer, u32 dstBasePtr, int dstStride, int &dstX, int &dstY, u32 srcBasePtr, int srcStride, int &srcX, int &srcY, int &srcWidth, int &srcHeight, int &dstWidth, int &dstHeight, int bpp) const;

	void SetNumExtraFBOs(int num);

	inline bool ShouldDownloadUsingCPU(const VirtualFramebuffer *vfb) const;

	// Used by ReadFramebufferToMemory and later framebuffer block copies
	void BlitFramebuffer_(VirtualFramebuffer *dst, int dstX, int dstY, VirtualFramebuffer *src, int srcX, int srcY, int w, int h, int bpp, bool flip = false);
#ifndef USING_GLES2
	void PackFramebufferAsync_(VirtualFramebuffer *vfb);
#endif
	void PackFramebufferSync_(VirtualFramebuffer *vfb, int x, int y, int w, int h);

	// Used by DrawPixels
	unsigned int drawPixelsTex_;
	GEBufferFormat drawPixelsTexFormat_;
	int drawPixelsTexW_;
	int drawPixelsTexH_;

	u8 *convBuf_;
	u32 convBufSize_;
	GLSLProgram *draw2dprogram_;
	GLSLProgram *plainColorProgram_;
	GLSLProgram *postShaderProgram_;
	GLSLProgram *stencilUploadProgram_;
	int plainColorLoc_;
	int timeLoc_;

	TextureCache *textureCache_;
	ShaderManager *shaderManager_;
	TransformDrawEngine *transformDraw_;
	bool usePostShader_;
	bool postShaderAtOutputResolution_;

	// Used by post-processing shader
	std::vector<FBO *> extraFBOs_;

	bool resized_;
	bool gameUsesSequentialCopies_;

	struct TempFBO {
		FBO *fbo;
		int last_frame_used;
	};

	std::vector<VirtualFramebuffer *> bvfbs_; // blitting framebuffers (for download)
	std::map<u64, TempFBO> tempFBOs_;

	std::set<std::pair<u32, u32>> knownFramebufferRAMCopies_;

#ifndef USING_GLES2
	AsyncPBO *pixelBufObj_; //this isn't that large
	u8 currentPBO_;
#endif
};
