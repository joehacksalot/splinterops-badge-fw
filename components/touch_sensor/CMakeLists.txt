idf_component_register(
    SRCS "TouchSensor.c"
    INCLUDE_DIRS "."
    REQUIRES driver freertos log notification_dispatcher time_utils utilities badge_hw_profile
)

if(CONFIG_BADGE_TYPE_TRON)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE TRON_BADGE)
elseif(CONFIG_BADGE_TYPE_REACTOR)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE REACTOR_BADGE)
elseif(CONFIG_BADGE_TYPE_CREST)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE CREST_BADGE)
elseif(CONFIG_BADGE_TYPE_FMAN25)
  target_compile_definitions(${COMPONENT_LIB} PRIVATE FMAN25_BADGE)
else()
  target_compile_definitions(${COMPONENT_LIB} PRIVATE FMAN25_BADGE)
endif()
