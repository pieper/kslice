project(vtkSlicer${MODULE_NAME}ModuleLogic)

set(KIT ${PROJECT_NAME})

set(${KIT}_EXPORT_DIRECTIVE "VTK_SLICER_${MODULE_NAME_UPPER}_MODULE_LOGIC_EXPORT")

set(${KIT}_INCLUDE_DIRECTORIES
  ${CMAKE_SOURCE_DIR}/KSegmentor/include/
  ${CMAKE_SOURCE_DIR}/KViewerMain/include/
  ${CMAKE_SOURCE_DIR}/util/vrcl/include/
  )

set(${KIT}_SRCS
  vtkSlicer${MODULE_NAME}Logic.cxx
  vtkSlicer${MODULE_NAME}Logic.h
  vtkKSlice.cpp 
  vtkKSlice.h 
)

message(STATUS "Slicer_SOURCE_DIR is:  [ ${Slicer_SOURCE_DIR} ]...")

link_directories(${KSLICE_LIB_PATH})
set(${KIT}_TARGET_LIBRARIES
  ${ITK_LIBRARIES}
    KSegmentor  sfm_local_chanvese
  )

#-----------------------------------------------------------------------------
SlicerMacroBuildModuleLogic(
  NAME ${KIT}
  EXPORT_DIRECTIVE ${${KIT}_EXPORT_DIRECTIVE}
  INCLUDE_DIRECTORIES ${${KIT}_INCLUDE_DIRECTORIES}
  SRCS ${${KIT}_SRCS}
  TARGET_LIBRARIES ${${KIT}_TARGET_LIBRARIES}
  )
