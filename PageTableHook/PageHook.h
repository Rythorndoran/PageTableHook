#pragma once
namespace PageHook
{
	auto add_page_hook(void* target_function, void* hook_function, void** original_function) -> void;
}