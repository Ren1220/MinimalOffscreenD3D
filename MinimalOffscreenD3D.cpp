
#define WIN32_LEAN_AND_MEAN

#include <d3d11.h>
#pragma comment(lib, "d3d11")

#include <wincodec.h> // do zapisu obrazu: Windows Imaging Components (WIC)
#pragma comment(lib, "windowscodecs")

#include <atlbase.h> // CComPtr u�atwi prac� ze wska�nikami (RAII)

// binary_t pomocnik do �adowania zasob�w binarnych z dysku
#include <vector>
#include <fstream>  // ifstream
#include <iterator> // istream_iterator
struct binary_t : std::vector<unsigned char>
{
	binary_t(const char path[]) : vector( // konstruktor przyjmuje �cie�k� do pliku binarnego
		std::istream_iterator<value_type>(std::ifstream(path, std::ios::binary) >> std::noskipws),
		std::istream_iterator<value_type>())
	{}
};


int main()
{
	static const UINT IMAGE_WIDTH=640, IMAGE_HEIGHT=480, NUM_POINTS=4;

	// tworzymy urz�dzenie i kontekst Direct3D
	// - �eby wiedzie� co si� dzieje na karcie graficznej, mo�na w��czy�: 
	//   Debug -> Graphics -> DirectX Control Panel -> Debug Layer -> Force On (wymusi� warstw� debug)

	CComPtr<ID3D11Device> dev; 
	CComPtr<ID3D11DeviceContext> ctx;
	::D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, 0, 0, nullptr, 0, D3D11_SDK_VERSION, &dev, 0, &ctx);

	CComPtr<ID3D11Texture2D> target, image; // tworzymy tekstur� wynikow� i obraz wyj�ciowy
	D3D11_TEXTURE2D_DESC desc = {};
	desc.Width  = IMAGE_WIDTH;                 // wymiary obrazka
	desc.Height = IMAGE_HEIGHT;
	desc.ArraySize = 1;                        // trzeba ustawi� (trzeci wymiar tekstury?)...
	desc.SampleDesc.Count = 1;                 // wielokrotne pr�bkowanie
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;  // albo np. DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	desc.BindFlags = D3D11_BIND_RENDER_TARGET; // do czego b�dziemy potrzebowa� tej tekstury
	dev->CreateTexture2D(&desc, nullptr, &target);

	desc.BindFlags = 0;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	dev->CreateTexture2D(&desc, nullptr, &image);


	// tworzymy widok na tekstur� i ustawiamy jako "wyj�cie" z shadera

	CComPtr<ID3D11RenderTargetView> view;
	dev->CreateRenderTargetView(target, nullptr, &view); // nie podajemy D3D11_RENDER_TARGET_VIEW_DESC, wymiary b�d� skopiowane z tekstury
	ctx->OMSetRenderTargets(1, &view.p, nullptr); // je�li potrzebujemy bufor g��bi to trzeba jeszcze ustawi� ID3D11DepthStencilState

	// ustawiamy viewport

	D3D11_VIEWPORT viewport = {};
	viewport.Width  = IMAGE_WIDTH;
	viewport.Height = IMAGE_HEIGHT;
	viewport.MaxDepth = D3D11_MAX_DEPTH; // trzeba ustawi�
	ctx->RSSetViewports(1, &viewport);

	// wczytujemy i ustawiamy shadery
	// - mo�emy kompilowa� je ze �r�de� ::D3D10CompileShader(), co wymaga�oby linkowania z d3d10.lib
	// - VS od 2012 pozwala na budowanie shader�w, mo�na ustawi� tworzenie nag��wka z wykonywalnym kodem w postaci tablicy znak�w
	// - tutaj �adujemy do pami�ci kod skompilowanego shadera z pliku *.cso (compiled shader object) i tworzymy shader

	CComPtr<ID3D11VertexShader> vertex_shader;
	const binary_t vertex_code = "VertexShader.cso";
	dev->CreateVertexShader(vertex_code.data(), vertex_code.size(), nullptr, &vertex_shader);
	ctx->VSSetShader(vertex_shader, nullptr, 0);


	CComPtr<ID3D11PixelShader>  pixel_shader;
	const binary_t pixel_code = "PixelShader.cso";
	dev->CreatePixelShader(pixel_code.data(), pixel_code.size(), nullptr, &pixel_shader);
	ctx->PSSetShader(pixel_shader, nullptr, 0);

	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP); // b�dziemy rysowa� tr�jk�ty "ciurkiem"
	
	// rysujemy

	ctx->Draw(NUM_POINTS, 0); // rysujemy cztery wierzcho�ki (bez danych wej�ciowych, shader wyliczy wsp�rz�dne)

	// sp�ukujemy :)

	ctx->Flush(); // to jest wa�ne, zamiast IDXGISwapchain::Present();

	// kopiujemy piksele z tekstury wynikowej do obrazu, z kt�rego mo�emy je odczyta�

	ctx->CopyResource(image, target); // tu si� lepiej nie pomyli�

	// mapujemy zas�b do pami�ci, �eby dosta� si� do pikseli obrazu - WA�NE �eby potem j� odmapowa�!
	D3D11_MAPPED_SUBRESOURCE resource = {};
	static const UINT resource_id = D3D11CalcSubresource(0, 0, 0);
	ctx->Map(image, resource_id, D3D11_MAP_READ, 0, &resource);
	resource.pData;

	// tworzymy fabryk� WIC i bitmap� ze zmapowanej pami�ci

	CoInitialize(nullptr); // wa�ne, �eby zainicializowa� COM
	CComPtr<IWICImagingFactory> factory;
	factory.CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER);

	CComPtr<IWICBitmap> bitmap;
	factory->CreateBitmapFromMemory(
		IMAGE_WIDTH, IMAGE_HEIGHT, 
		GUID_WICPixelFormat32bppRGBA, resource.RowPitch, resource.DepthPitch, 
		(BYTE*)resource.pData, &bitmap);
	ctx->Unmap(image, resource_id); // nie zapomnijmy odmapowa� zasobu

	// inicjalizujemy enkoder i strumie� PNG

	CComPtr<IWICStream> stream;
	CComPtr<IWICBitmapEncoder> encoder;
	CComPtr<IWICBitmapFrameEncode> frame;

	factory->CreateStream(&stream);
	stream->InitializeFromFilename(L"image.png", GENERIC_WRITE);

	factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder);
	encoder->Initialize(stream, WICBitmapEncoderNoCache);
	encoder->CreateNewFrame(&frame, nullptr);
	frame->Initialize(nullptr);

	frame->WriteSource(bitmap, nullptr); // nie przekazujemy WICRect, zapisany b�dzie ca�y obraz

	frame->Commit();
	encoder->Commit();

	// wypada�oby deinicjalizowa� COM, ale trzeba wcze�niej zniszczy� wszystkie obiekty WIC
	//CoUninitialize();

	return 0;
}

