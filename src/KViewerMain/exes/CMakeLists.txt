set(KV_LIBS KSlice QVTK  ${QT_LIBRARIES}  vtkRendering  vtkGraphics
         vtkIO  vtkCommon   ) #${OpenCV_LIBS}

set(KV_SRC      ${CMAKE_CURRENT_SOURCE_DIR}/../../kslice_wrap/Logic/vtkKSlice.cpp
                ${CMAKE_CURRENT_SOURCE_DIR}/../../kslice_wrap/Logic/vtkKSlice.h
                ${CMAKE_BINARY_DIR}/kslice_wrap/Logic/vtkSlicerKSliceModuleLogicExport.h ) #a file called vtkSlicerKSliceModuleLogicExport.h gets created


include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../../kslice_wrap/Logic/
                     ${CMAKE_BINARY_DIR}/kslice_wrap/Logic/)

if(WIN32)
	message(STATUS "skipping KViewer standalone on windows")
else()
	add_executable( KViewer_desktop KViewer_desktop.cpp ${KV_SRC} )
	target_link_libraries( KViewer_desktop ${KV_LIBS})
endif(WIN32)