# Badge hardware profile component

# Collect per-badge sources and include dirs
set(BADGE_HW_SRCS "BadgeHwProfile.c" "BadgeType.c")
set(BADGE_HW_INCLUDES ".")

if(CONFIG_BADGE_TYPE_TRON)
  list(APPEND BADGE_HW_SRCS "tron/BadgeProfile.c")
  list(APPEND BADGE_HW_INCLUDES "tron")
elseif(CONFIG_BADGE_TYPE_REACTOR)
  list(APPEND BADGE_HW_SRCS "reactor/BadgeProfile.c")
  list(APPEND BADGE_HW_INCLUDES "reactor")
elseif(CONFIG_BADGE_TYPE_CREST)
  list(APPEND BADGE_HW_SRCS "crest/BadgeProfile.c")
  list(APPEND BADGE_HW_INCLUDES "crest")
elseif(CONFIG_BADGE_TYPE_FMAN25)
  list(APPEND BADGE_HW_SRCS "fman25/BadgeProfile.c")
  list(APPEND BADGE_HW_INCLUDES "fman25")
else()
  # Default to FMAN25 if none selected
  list(APPEND BADGE_HW_SRCS "fman25/BadgeProfile.c")
  list(APPEND BADGE_HW_INCLUDES "fman25")
endif()

idf_component_register(
  SRCS ${BADGE_HW_SRCS}
  INCLUDE_DIRS ${BADGE_HW_INCLUDES}
  REQUIRES 
)

# Also expose the traditional per-badge compile definitions for consumers that rely on them
if(CONFIG_BADGE_TYPE_TRON)
  target_compile_definitions(${COMPONENT_LIB} INTERFACE TRON_BADGE)
elseif(CONFIG_BADGE_TYPE_REACTOR)
  target_compile_definitions(${COMPONENT_LIB} INTERFACE REACTOR_BADGE)
elseif(CONFIG_BADGE_TYPE_CREST)
  target_compile_definitions(${COMPONENT_LIB} INTERFACE CREST_BADGE)
elseif(CONFIG_BADGE_TYPE_FMAN25)
  target_compile_definitions(${COMPONENT_LIB} INTERFACE FMAN25_BADGE)
else()
  target_compile_definitions(${COMPONENT_LIB} INTERFACE FMAN25_BADGE)
endif()
