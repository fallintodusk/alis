"""
Configuration for Blueprint JSON Converter

Centralized configuration following Dependency Inversion Principle.
All hardcoded paths and constants are defined here.
"""
import os

try:
    import unreal
    PROJECT_DIR = unreal.SystemLibrary.get_project_directory()
except (ImportError, RuntimeError):
    # Fallback for testing outside UE
    PROJECT_DIR = os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__))))

# Default paths
DEFAULT_ASSET_PATH = "/Game/Project/Placeables/Environment/BP_SunSky_Child.BP_SunSky_Child"
OUTPUT_DIR = os.path.join(PROJECT_DIR, "Saved", "AI_Snapshots")

# File naming
FILENAME_TEMPLATE = "blueprint_{timestamp}.json"
TIMESTAMP_FORMAT = "%Y%m%d_%H%M%S"

# Export settings
MAX_ARRAY_ITEMS = 10  # Limit array exports to prevent huge JSONs
SKIP_PRIVATE_PROPERTIES = True  # Skip properties starting with '_'
SKIP_METHODS = True  # Skip callable attributes

# Property filtering
PROTECTED_PROPERTY_PREFIXES = ['_', '__']
EXCLUDED_PROPERTY_TYPES = ['method', 'builtin_function_or_method']

# Logging
VERBOSE_LOGGING = True
LOG_SUCCESS_COUNT = True
LOG_ERROR_COUNT = True
