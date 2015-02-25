#include "RocketDocument2D.h"

#include <Rocket\Core.h>
#include "Context.h"
#include "CoreEvents.h"
#include "Graphics.h"
#include "GraphicsEvents.h"
#include "Node.h"
#include "ResourceCache.h"

namespace Urho3D
{
	namespace Rocket
	{
		RocketDocument2D::RocketDocument2D(Urho3D::Context* context) :
			Component(context)
		{
			Urho3D::Graphics* graphics = GetSubsystem<Urho3D::Graphics>();

			noTextureVS = graphics->GetShader(Urho3D::VS, "Basic", "VERTEXCOLOR");
			noTexturePS = graphics->GetShader(Urho3D::PS, "Basic", "VERTEXCOLOR");

			diffTextureVS = graphics->GetShader(Urho3D::VS, "Basic", "DIFFMAP VERTEXCOLOR");
			diffTexturePS = graphics->GetShader(Urho3D::PS, "Basic", "DIFFMAP VERTEXCOLOR");

			_rocketContext = std::shared_ptr<::Rocket::Core::Context>(
				::Rocket::Core::CreateContext("main", 
				::Rocket::Core::Vector2i(graphics->GetWidth(), graphics->GetHeight()), 
				this));

			::Rocket::Core::FontDatabase::LoadFontFace("Data/Fonts/Delicious-Bold.otf");
			::Rocket::Core::FontDatabase::LoadFontFace("Data/Fonts/Delicious-BoldItalic.otf");
			::Rocket::Core::FontDatabase::LoadFontFace("Data/Fonts/Delicious-Italic.otf");
			::Rocket::Core::FontDatabase::LoadFontFace("Data/Fonts/Delicious-Roman.otf");

			::Rocket::Core::ElementDocument *Document = _rocketContext->LoadDocument("Data/UI/rocketDemo.html");

			if (Document)
			{
				Document->Show();
				//Document->RemoveReference();
			};

			SubscribeToEvent(Urho3D::E_UPDATE, HANDLER(RocketDocument2D, HandleUpdate));
			SubscribeToEvent(Urho3D::E_ENDRENDERING, HANDLER(RocketDocument2D, HandlePostRender));
		}

		RocketDocument2D::~RocketDocument2D()
		{

		}

		void RocketDocument2D::RegisterObject(Urho3D::Context* context)
		{
			context->RegisterFactory<RocketDocument2D>();
		}

		void RocketDocument2D::AddReference()
		{
			AddRef();
		}

		void RocketDocument2D::RemoveReference()
		{
			ReleaseRef();
		}

		// Called by Rocket when it wants to render geometry that it does not wish to optimise.
		void RocketDocument2D::RenderGeometry(::Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, const ::Rocket::Core::TextureHandle texture, const ::Rocket::Core::Vector2f& translation)
		{
		}

		// Called by Rocket when it wants to compile geometry it believes will be static for the forseeable future.		
		::Rocket::Core::CompiledGeometryHandle RocketDocument2D::CompileGeometry(::Rocket::Core::Vertex* vertices, int num_vertices, int* indices, int num_indices, const ::Rocket::Core::TextureHandle texture)
		{
			VertexBuffer* vBuff = new VertexBuffer(context_);
			if (texture) {
				vBuff->SetSize(num_vertices, MASK_POSITION | MASK_COLOR | MASK_TEXCOORD1, true);
			}
			else {
				vBuff->SetSize(num_vertices, MASK_POSITION | MASK_COLOR, true);
			}

			float* dest = (float*)vBuff->Lock(0, num_vertices, true);
			for (int i = 0; i < num_vertices; ++i) {
				*dest++ = vertices[i].position.x;
				*dest++ = vertices[i].position.y;
				*dest++ = 0.f;

				Color color(vertices[i].colour.red / 255.f, vertices[i].colour.green / 255.f, vertices[i].colour.blue / 255.f, vertices[i].colour.alpha / 255.f);
				*((unsigned*)dest) = color.ToUInt(); dest++;

				if (texture) {
					*dest++ = vertices[i].tex_coord.x;
					*dest++ = vertices[i].tex_coord.y;
				}
			}
			vBuff->Unlock();

			IndexBuffer*  iBuff = new IndexBuffer(context_);
			iBuff->SetSize(num_indices, true);

			// indices
			unsigned* indices_stream = (unsigned*)iBuff->Lock(0, num_indices, true);
			for (int i = 0; i < num_indices; ++i)
				indices_stream[i] = (unsigned int)indices[i];

			iBuff->Unlock();

			Texture2D* tex = texture ? textures.at(texture - 1) : NULL;

			Core::Urho3DCompiledGeometry* geom = new Core::Urho3DCompiledGeometry();
			geom->iBuff = iBuff;
			geom->vBuff = vBuff;
			geom->texture = tex;

			return (::Rocket::Core::CompiledGeometryHandle) geom;
		}

		// Called by Rocket when it wants to render application-compiled geometry.		
		void RocketDocument2D::RenderCompiledGeometry(::Rocket::Core::CompiledGeometryHandle geometry, const ::Rocket::Core::Vector2f& translation)
		{
			Urho3D::Graphics* graphics = GetSubsystem<Urho3D::Graphics>();

			Core::Urho3DCompiledGeometry* geom = (Core::Urho3DCompiledGeometry*)geometry;

			//Setup projection matrix
			Vector2 invScreenSize(1.0f / (float)graphics->GetWidth(), 1.0f / (float)graphics->GetHeight());
			Vector2 scale(2.0f * invScreenSize.x_, -2.0f * invScreenSize.y_);
			Vector2 offset(-1.0f, 1.0f);

			Matrix4 projection(Matrix4::IDENTITY);
			projection.m00_ = scale.x_;
			projection.m03_ = offset.x_;
			projection.m11_ = scale.y_;
			projection.m13_ = offset.y_;
			projection.m22_ = 1.0f;
			projection.m23_ = 0.0f;
			projection.m33_ = 1.0f;

			//Setup graphics rendering states
			graphics->ClearParameterSources();
			graphics->SetCullMode(CULL_CW);
			graphics->SetDepthTest(CMP_ALWAYS);
			graphics->SetDepthWrite(false);
			graphics->SetStencilTest(false);
			graphics->ResetRenderTargets();
			graphics->SetBlendMode(BLEND_ALPHA);

			//Bind buffers
			graphics->SetVertexBuffer(geom->vBuff);
			graphics->SetIndexBuffer(geom->iBuff);

			ShaderVariation* ps = 0;
			ShaderVariation* vs = 0;

			if (geom->texture) {
				graphics->SetTexture(0, geom->texture);
				ps = diffTexturePS;
				vs = diffTextureVS;
			}
			else {
				graphics->SetTexture(0, NULL);
				ps = noTexturePS;
				vs = noTextureVS;
			}

			//Apply translation
			Matrix3x4 trans(Matrix3x4::IDENTITY);
			trans.SetTranslation(Vector3(translation.x, translation.y, 0.f));

			//Pass shader params
			graphics->SetShaders(vs, ps);
			if (graphics->NeedParameterUpdate(SP_OBJECTTRANSFORM, this))
				graphics->SetShaderParameter(VSP_MODEL, trans);
			if (graphics->NeedParameterUpdate(SP_CAMERA, this))
				graphics->SetShaderParameter(VSP_VIEWPROJ, projection);
			if (graphics->NeedParameterUpdate(SP_MATERIAL, this))
				graphics->SetShaderParameter(PSP_MATDIFFCOLOR, Color(1.0f, 1.0f, 1.0f, 1.0f));

			//Draw indexed geometry
			graphics->Draw(TRIANGLE_LIST, 0, geom->iBuff->GetIndexCount(), 0, geom->vBuff->GetVertexCount());
		}

		// Called by Rocket when it wants to release application-compiled geometry.		
		void RocketDocument2D::ReleaseCompiledGeometry(::Rocket::Core::CompiledGeometryHandle geometry)
		{
			Core::Urho3DCompiledGeometry* geom = (Core::Urho3DCompiledGeometry*)geometry;
			delete geom->iBuff;
			delete geom->vBuff;
			delete geom;
		}

		// Called by Rocket when it wants to enable or disable scissoring to clip content.		
		void RocketDocument2D::EnableScissorRegion(bool enable)
		{
			Urho3D::Graphics* graphics = GetSubsystem<Urho3D::Graphics>();

			if (enable)
				graphics->SetScissorTest(true, scissors);
			else
				graphics->SetScissorTest(false);
		}

		// Called by Rocket when it wants to change the scissor region.		
		void RocketDocument2D::SetScissorRegion(int x, int y, int width, int height)
		{
			scissors.left_ = x;
			scissors.top_ = y;
			scissors.bottom_ = height;
			scissors.right_ = width;
		}

		// Called by Rocket when a texture is required by the library.		
		bool RocketDocument2D::LoadTexture(::Rocket::Core::TextureHandle& texture_handle, ::Rocket::Core::Vector2i& texture_dimensions, const ::Rocket::Core::String& source)
		{
			Texture2D* tex = context_->GetSubsystem<Urho3D::ResourceCache>()->GetResource<Urho3D::Texture2D>(source.CString());
			if (tex) {
				texture_dimensions.x = tex->GetWidth();
				texture_dimensions.y = tex->GetHeight();
			}

			textures.push_back(tex);
			texture_handle = (::Rocket::Core::TextureHandle)textures.size();   // indexing from 1, 0 means no texture

			return true;
		}

		// Called by Rocket when a texture is required to be built from an internally-generated sequence of pixels.
		bool RocketDocument2D::GenerateTexture(::Rocket::Core::TextureHandle& texture_handle, const ::Rocket::Core::byte* source, const ::Rocket::Core::Vector2i& source_dimensions)
		{
			SharedPtr<Image> img = SharedPtr<Image>(new Image(context_));
			img->SetSize(source_dimensions.x, source_dimensions.y, 4);
			img->SetData(source);
			Texture2D* tex = new Texture2D(context_);
			tex->SetData(img);

			textures.push_back(tex);
			texture_handle = (::Rocket::Core::TextureHandle)textures.size();		// indexing from 1, 0 means no texture

			return true;
		}

		// Called by Rocket when a loaded texture is no longer required.		
		void RocketDocument2D::ReleaseTexture(::Rocket::Core::TextureHandle texture_handle)
		{
			Texture2D *tex = textures.at(texture_handle - 1);
			delete tex;
			tex = NULL;
		}

		float RocketDocument2D::GetHorizontalTexelOffset()
		{
#ifdef USE_OPENGL
			return 0.f;
#else
			return 0.5f;
#endif
		}

		float RocketDocument2D::GetVerticalTexelOffset()
		{
#ifdef USE_OPENGL
			return 0.f;
#else
			return 0.5f;
#endif
		}

		void RocketDocument2D::HandleUpdate(Urho3D::StringHash eventType, Urho3D::VariantMap& eventData)
		{      
			_rocketContext->Update();
		}

		void RocketDocument2D::HandlePostRender(Urho3D::StringHash eventType, Urho3D::VariantMap &eventData)
		{
			_rocketContext->Render();
		}

	}
}