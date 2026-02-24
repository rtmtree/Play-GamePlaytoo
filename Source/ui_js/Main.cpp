#include <cstdio>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "Ps2VmJs.h"
#include "GSH_OpenGLJs.h"
#include "GSH_WebGPUJs.h"
#include "sound/SH_OpenAL/SH_OpenALProxy.h"
#include "input/PH_GenericInput.h"
#include "InputProviderEmscripten.h"
#include "ui_shared/StatsManager.h"
#include "DefaultAppConfig.h"
#include "filesystem_def.h"

CPs2VmJs* g_virtualMachine = nullptr;
CGSHandler::NewFrameEvent::Connection g_gsNewFrameConnection;
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE g_context = 0;
std::shared_ptr<CInputProviderEmscripten> g_inputProvider;
CSH_OpenAL* g_soundHandler = nullptr;

int main(int argc, const char** argv)
{
	printf("Play! - Version %s\r\n", PLAY_VERSION);
	return 0;
}

EM_BOOL keyboardCallback(int eventType, const EmscriptenKeyboardEvent* keyEvent, void* userData)
{
	if(keyEvent->repeat)
	{
		return true;
	}
	switch(eventType)
	{
	case EMSCRIPTEN_EVENT_KEYDOWN:
		g_inputProvider->OnKeyDown(keyEvent->code);
		break;
	case EMSCRIPTEN_EVENT_KEYUP:
		g_inputProvider->OnKeyUp(keyEvent->code);
		break;
	}
	return true;
}

extern "C" void initVm()
{
	EmscriptenWebGLContextAttributes attr;
	emscripten_webgl_init_context_attributes(&attr);
	attr.majorVersion = 2;
	attr.minorVersion = 0;
	attr.alpha = false;
	attr.explicitSwapControl = true;
	g_context = emscripten_webgl_create_context("#outputCanvas", &attr);
	if(g_context <= 0)
	{
		// Try WebGL 1.0 as fallback if WebGL 2.0 fails
		attr.majorVersion = 1;
		attr.minorVersion = 0;
		g_context = emscripten_webgl_create_context("#outputCanvas", &attr);
		if(g_context <= 0)
		{
			printf("ERROR: Failed to create WebGL context. Error code: %lu\n", static_cast<unsigned long>(g_context));
			printf("Canvas #outputCanvas may not exist or WebGL is not available.\n");
			// Throw an exception that JavaScript can catch
			emscripten_throw_string("WebGL context creation failed");
			return;
		}
	}

	g_virtualMachine = new CPs2VmJs();
	g_virtualMachine->Initialize();
	g_virtualMachine->CreateGSHandler(CGSH_OpenGLJs::GetFactoryFunction(g_context));

	{
		//Size here needs to match the size of the canvas in HTML file.
		//Reduced resolution from 640x480 to 480x360 for low RAM device compatibility

		CGSHandler::PRESENTATION_PARAMS presentationParams;
		presentationParams.mode = CGSHandler::PRESENTATION_MODE_FIT;
		presentationParams.windowWidth = 480;
		presentationParams.windowHeight = 360;

		g_virtualMachine->m_ee->m_gs->SetPresentationParams(presentationParams);
	}

	{
		g_virtualMachine->CreatePadHandler(CPH_GenericInput::GetFactoryFunction());
		auto padHandler = static_cast<CPH_GenericInput*>(g_virtualMachine->GetPadHandler());
		auto& bindingManager = padHandler->GetBindingManager();

		g_inputProvider = std::make_shared<CInputProviderEmscripten>();
		bindingManager.RegisterInputProvider(g_inputProvider);

		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::START, CInputProviderEmscripten::MakeBindingTarget("Enter"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::SELECT, CInputProviderEmscripten::MakeBindingTarget("Backspace"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_LEFT, CInputProviderEmscripten::MakeBindingTarget("ArrowLeft"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_RIGHT, CInputProviderEmscripten::MakeBindingTarget("ArrowRight"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_UP, CInputProviderEmscripten::MakeBindingTarget("ArrowUp"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_DOWN, CInputProviderEmscripten::MakeBindingTarget("ArrowDown"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::SQUARE, CInputProviderEmscripten::MakeBindingTarget("KeyA"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::CROSS, CInputProviderEmscripten::MakeBindingTarget("KeyZ"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::TRIANGLE, CInputProviderEmscripten::MakeBindingTarget("KeyS"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::CIRCLE, CInputProviderEmscripten::MakeBindingTarget("KeyX"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::L1, CInputProviderEmscripten::MakeBindingTarget("Key1"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::L2, CInputProviderEmscripten::MakeBindingTarget("Key2"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::L3, CInputProviderEmscripten::MakeBindingTarget("Key3"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::R1, CInputProviderEmscripten::MakeBindingTarget("Key8"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::R2, CInputProviderEmscripten::MakeBindingTarget("Key9"));
		bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::R3, CInputProviderEmscripten::MakeBindingTarget("Key0"));

		bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_LEFT_X,
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyF"),
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyH"));
		bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_LEFT_Y,
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyT"),
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyG"));

		bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_RIGHT_X,
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyJ"),
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyL"));
		bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_RIGHT_Y,
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyI"),
		                                       CInputProviderEmscripten::MakeBindingTarget("KeyK"));
	}

	{
		g_soundHandler = new CSH_OpenAL();
		g_virtualMachine->CreateSoundHandler(CSH_OpenALProxy::GetFactoryFunction(g_soundHandler));
	}

	g_gsNewFrameConnection = g_virtualMachine->GetGSHandler()->OnNewFrame.Connect(std::bind(&CStatsManager::OnGsNewFrame, &CStatsManager::GetInstance(), std::placeholders::_1));

	EMSCRIPTEN_RESULT result = EMSCRIPTEN_RESULT_SUCCESS;

	result = emscripten_set_keydown_callback("#outputCanvas", nullptr, false, &keyboardCallback);
	assert(result == EMSCRIPTEN_RESULT_SUCCESS);

	result = emscripten_set_keyup_callback("#outputCanvas", nullptr, false, &keyboardCallback);
	assert(result == EMSCRIPTEN_RESULT_SUCCESS);
}

EM_JS(WGPUDevice, getDeviceHandle, (emscripten::EM_VAL device_val, emscripten::EM_VAL adapter_val), {
	var valObj = typeof Emval !== 'undefined' ? Emval : (typeof EmscriptenVal !== 'undefined' ? EmscriptenVal : null);
	if (!valObj) {
		console.error('getDeviceHandle: Emval/EmscriptenVal is undefined!');
		return 0;
	}
	var device = valObj.toValue(device_val);
	var adapter = adapter_val ? valObj.toValue(adapter_val) : null;
	
	console.log('getDeviceHandle: device object retrieved:', device);
	if (device && device.constructor) console.log('getDeviceHandle: device constructor name:', device.constructor.name);
	if (adapter) {
		console.log('getDeviceHandle: adapter object retrieved:', adapter);
		if (adapter.constructor) console.log('getDeviceHandle: adapter constructor name:', adapter.constructor.name);
	}

	if (!device) {
		console.error('getDeviceHandle: Failed to convert val to JS device object');
		return 0;
	}
	
	var webgpu = Module['WebGPU'];
	if (webgpu) {
		console.log('getDeviceHandle: Module.WebGPU exists');
		
		// In some versions, we might need to import the adapter first
		if (adapter && typeof webgpu['importJsAdapter'] === 'function') {
			try {
				var adapterHandle = webgpu['importJsAdapter'](adapter);
				console.log('getDeviceHandle: Imported adapter, handle:', adapterHandle);
			} catch(e) {
				console.warn('getDeviceHandle: Failed to import adapter:', e);
			}
		}

		// Try direct import
		if (typeof webgpu['importJsDevice'] === 'function') {
			try {
				var handle = webgpu['importJsDevice'](device);
				if (handle) {
					console.log('getDeviceHandle: Successfully imported device directly, handle:', handle);
					return handle;
				}
				console.warn('getDeviceHandle: importJsDevice returned 0');
			} catch(e) {
				console.warn('getDeviceHandle: importJsDevice threw:', e);
			}
		}
		
		// Try via mgr (manager)
		if (webgpu['mgr'] && typeof webgpu['mgr']['importJsDevice'] === 'function') {
			try {
				var handle = webgpu['mgr']['importJsDevice'](device);
				if (handle) {
					console.log('getDeviceHandle: Successfully imported device via mgr, handle:', handle);
					return handle;
				}
			} catch(e) {
				console.warn('getDeviceHandle: importJsDevice via mgr threw:', e);
			}
		}
		
		// Try via Internals
		if (webgpu['Internals'] && typeof webgpu['Internals']['importJsDevice'] === 'function') {
			try {
				var handle = webgpu['Internals']['importJsDevice'](device);
				if (handle) {
					console.log('getDeviceHandle: Successfully imported device via Internals, handle:', handle);
					return handle;
				}
			} catch(e) {
				console.warn('getDeviceHandle: importJsDevice via Internals threw:', e);
			}
		}

		console.error('getDeviceHandle: Failed to find a valid importJsDevice function or it returned 0');
		console.log('Available WebGPU keys:', Object.keys(webgpu));
	} else {
		console.error('getDeviceHandle: Module.WebGPU is missing');
	}
	
	return 0;
});

EM_JS(int, canCanvasBeAccessed, (emscripten::EM_VAL canvas_val), {
	try {
		var valObj = typeof Emval !== 'undefined' ? Emval : (typeof EmscriptenVal !== 'undefined' ? EmscriptenVal : null);
		var canvas = (canvas_val && valObj) ? valObj.toValue(canvas_val) : Module.canvas;

		if (!canvas) {
			console.warn('canCanvasBeAccessed: canvas is not available');
			return 0;
		}
		
		console.log('canCanvasBeAccessed: Canvas is accessible');
		return 1;
	} catch(e) {
		console.error('canCanvasBeAccessed: Exception checking canvas:', e);
		return 0;
	}
});

EM_JS(int, isWorker, (), {
	return (typeof window === 'undefined' && typeof importScripts === 'function') ? 1 : 0;
});

extern "C" void initVmWebGPU(emscripten::val device, emscripten::val adapter, emscripten::val canvas, std::string backend)
{
	printf("DEBUG: initVmWebGPU called with backend: %s\n", backend.c_str());
	printf("DEBUG: Running in %s thread\n", isWorker() ? "WORKER" : "MAIN");
	
	// Validate canvas accessibility before attempting WebGPU
	if (!canCanvasBeAccessed(canvas.as_handle())) {
		printf("ERROR: Canvas is not accessible - cannot initialize WebGPU\n");
		printf("HINT: WebGPU requires access to a canvas element.\n");
		emscripten_throw_string("Canvas not accessible - WebGPU initialization aborted");
		return;
	}
	
	printf("DEBUG: Canvas accessibility verified\n");
	
	try {
		WGPUDevice deviceHandle = getDeviceHandle(device.as_handle(), adapter.as_handle());
		if (!deviceHandle) {
			printf("ERROR: Failed to convert JS GPUDevice to WGPUDevice handle\n");
			throw std::runtime_error("Failed to convert JS GPUDevice to WGPUDevice handle");
		}
		
		printf("DEBUG: Creating WebGPU Instance and Surface\n");
		wgpu::InstanceDescriptor instanceDesc = {};
		wgpu::Instance instance = wgpu::CreateInstance(&instanceDesc);
		
		if (!instance) {
			printf("ERROR: Failed to create WebGPU instance\n");
			throw std::runtime_error("Failed to create WebGPU instance");
		}
		printf("DEBUG: Instance created: %p\n", (void*)instance.Get());

		wgpu::Surface surface = nullptr;

		// Try to create surface using the selector
		// In some versions of Dawn, the name is SurfaceDescriptorFromCanvasHTMLSelector
		// and in others it's EmscriptenSurfaceSourceCanvasHTMLSelector.
		// We'll use the latter as it was previously used, but make sure it's fully initialized.
		wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector canvasDesc = {};
		canvasDesc.selector = "#outputCanvas";
		
		wgpu::SurfaceDescriptor surfaceDesc = {};
		surfaceDesc.nextInChain = &canvasDesc;
		
		printf("DEBUG: Attempting to create surface with selector: #outputCanvas\n");
		surface = instance.CreateSurface(&surfaceDesc);
		
		if (!surface) {
			printf("ERROR: Failed to create WebGPU surface with selector\n");
			// If it failed, maybe we are in a worker and need to try something else?
			// But for now, let's just fail and log.
			throw std::runtime_error("Failed to create WebGPU surface");
		}
		printf("DEBUG: Surface created: %p\n", (void*)surface.Get());

		printf("DEBUG: Creating PS2 VM\n");
		g_virtualMachine = new CPs2VmJs();
		g_virtualMachine->Initialize();
		
		printf("DEBUG: Creating WebGPU GS handler\n");
		g_virtualMachine->CreateGSHandler(CGSH_WebGPUJs::GetFactoryFunction(wgpu::Device::Acquire(deviceHandle), instance, surface, backend));
		
		printf("DEBUG: WebGPU GS handler created successfully\n");
		
		{
			CGSHandler::PRESENTATION_PARAMS presentationParams;
			presentationParams.mode = CGSHandler::PRESENTATION_MODE_FIT;
			presentationParams.windowWidth = 480;
			presentationParams.windowHeight = 360;

			g_virtualMachine->m_ee->m_gs->SetPresentationParams(presentationParams);
		}

		{
			g_virtualMachine->CreatePadHandler(CPH_GenericInput::GetFactoryFunction());
			auto padHandler = static_cast<CPH_GenericInput*>(g_virtualMachine->GetPadHandler());
			auto& bindingManager = padHandler->GetBindingManager();

			g_inputProvider = std::make_shared<CInputProviderEmscripten>();
			bindingManager.RegisterInputProvider(g_inputProvider);

			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::START, CInputProviderEmscripten::MakeBindingTarget("Enter"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::SELECT, CInputProviderEmscripten::MakeBindingTarget("Backspace"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_LEFT, CInputProviderEmscripten::MakeBindingTarget("ArrowLeft"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_RIGHT, CInputProviderEmscripten::MakeBindingTarget("ArrowRight"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_UP, CInputProviderEmscripten::MakeBindingTarget("ArrowUp"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::DPAD_DOWN, CInputProviderEmscripten::MakeBindingTarget("ArrowDown"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::SQUARE, CInputProviderEmscripten::MakeBindingTarget("KeyA"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::CROSS, CInputProviderEmscripten::MakeBindingTarget("KeyZ"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::TRIANGLE, CInputProviderEmscripten::MakeBindingTarget("KeyS"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::CIRCLE, CInputProviderEmscripten::MakeBindingTarget("KeyX"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::L1, CInputProviderEmscripten::MakeBindingTarget("Key1"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::L2, CInputProviderEmscripten::MakeBindingTarget("Key2"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::L3, CInputProviderEmscripten::MakeBindingTarget("Key3"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::R1, CInputProviderEmscripten::MakeBindingTarget("Key8"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::R2, CInputProviderEmscripten::MakeBindingTarget("Key9"));
			bindingManager.SetSimpleBinding(0, PS2::CControllerInfo::R3, CInputProviderEmscripten::MakeBindingTarget("Key0"));

			bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_LEFT_X,
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyF"),
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyH"));
			bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_LEFT_Y,
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyT"),
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyG"));

			bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_RIGHT_X,
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyJ"),
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyL"));
			bindingManager.SetSimulatedAxisBinding(0, PS2::CControllerInfo::ANALOG_RIGHT_Y,
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyI"),
			                                       CInputProviderEmscripten::MakeBindingTarget("KeyK"));
		}

		{
			g_soundHandler = new CSH_OpenAL();
			g_virtualMachine->CreateSoundHandler(CSH_OpenALProxy::GetFactoryFunction(g_soundHandler));
		}

		g_gsNewFrameConnection = g_virtualMachine->GetGSHandler()->OnNewFrame.Connect(std::bind(&CStatsManager::OnGsNewFrame, &CStatsManager::GetInstance(), std::placeholders::_1));

		EMSCRIPTEN_RESULT result = EMSCRIPTEN_RESULT_SUCCESS;

		result = emscripten_set_keydown_callback("#outputCanvas", nullptr, false, &keyboardCallback);
		assert(result == EMSCRIPTEN_RESULT_SUCCESS);

		result = emscripten_set_keyup_callback("#outputCanvas", nullptr, false, &keyboardCallback);
		assert(result == EMSCRIPTEN_RESULT_SUCCESS);
		
		printf("DEBUG: initVmWebGPU completed successfully\n");
	} 
	catch (const std::exception& e) {
		printf("ERROR: Exception during WebGPU initialization: %s\n", e.what());
		printf("ERROR: WebGPU handler creation failed (likely canvas not accessible from worker thread)\n");
		printf("HINT: This may occur when WASM runs in a worker thread and cannot access the main thread's canvas.\n");
		emscripten_throw_string("WebGPU handler creation failed - canvas may not be accessible from worker thread");
	}
	catch (...) {
		printf("ERROR: Unknown exception during WebGPU handler creation\n");
		emscripten_throw_string("WebGPU handler creation failed - unknown error");
	}
}

void bootElf(std::string path)
{
	if(!g_virtualMachine)
	{
		printf("ERROR: bootElf called but virtual machine is null\n");
		return;
	}
	printf("DEBUG: Booting ELF from: %s\n", path.c_str());
	g_virtualMachine->BootElf(path);
}

void bootDiscImage(std::string path)
{
	if(!g_virtualMachine)
	{
		printf("ERROR: bootDiscImage called but virtual machine is null\n");
		return;
	}
	printf("DEBUG: Booting disc image from: %s\n", path.c_str());
	g_virtualMachine->BootDiscImage(path);
}

int getFrames()
{
	return CStatsManager::GetInstance().GetFrames();
}

void clearStats()
{
	CStatsManager::GetInstance().ClearStats();
}

bool saveState(std::string path)
{
	if(!g_virtualMachine)
	{
		return false;
	}
	fs::path statePath(path);
	auto future = g_virtualMachine->SaveState(statePath);
	return future.get();
}

bool loadState(std::string path)
{
	if(!g_virtualMachine)
	{
		return false;
	}
	fs::path statePath(path);
	auto future = g_virtualMachine->LoadState(statePath);
	return future.get();
}

void pauseVm()
{
	if(!g_virtualMachine)
	{
		return;
	}
	g_virtualMachine->Pause();
}

void resumeVm()
{
	if(!g_virtualMachine)
	{
		return;
	}
	g_virtualMachine->Resume();
}

int getStatus()
{
	if(!g_virtualMachine)
	{
		return 0; // STOPPED
	}
	return static_cast<int>(g_virtualMachine->GetStatus());
}

void setFrameskip(int count)
{
	CAppConfig::GetInstance().SetPreferenceInteger(PREF_CGSHANDLER_FRAMESKIP, count);
	if(g_virtualMachine && g_virtualMachine->GetGSHandler())
	{
		g_virtualMachine->GetGSHandler()->NotifyPreferencesChanged();
	}
}

EMSCRIPTEN_BINDINGS(Play)
{
	using namespace emscripten;

	function("initVm", &initVm);
	function("initVmWebGPU", &initVmWebGPU);
	function("bootElf", &bootElf);
	function("bootDiscImage", &bootDiscImage);
	function("getFrames", &getFrames);
	function("clearStats", &clearStats);
	function("saveState", &saveState);
	function("loadState", &loadState);
	function("pauseVm", &pauseVm);
	function("resumeVm", &resumeVm);
	function("getStatus", &getStatus);
	function("setFrameskip", &setFrameskip);
}
