include ( ../../settings.pro )

TEMPLATE = aux

#
# database backup/restore scripts are standalone
#
database_scripts.path = $${PREFIX}/share/mythtv
database_scripts.files = database/*
INSTALLS += database_scripts

#
# We leverage the capability of qmake to invoke an extra
# "compiler" to transform the python scripts and adjust
# python script shebangs.  Since the scripts depend on
# the python bindings, and when using_python_bindings is
# true we know we have the bindings and a usable python
# executable (verified as part of configure), we can use
# a python script to modify the shebang on scripts in the
# same way that the bindings setup.py will modify the
# shebangs for executable scripts (typically something of
# the form #!/usr/bin/pythonX).
#
# However, there is an exception for the Music metadata
# scripts, which do not require the python bindings.  We
# try another way to locate a python executable to be
# used to modify the shebangs that may be less accurate.
# It is expected that typically one will build the
# python bindings, making thise case less likely in
# practice.
#

using_bindings_python {

    PYTHON_SOURCES += hardwareprofile/*
    PYTHON_SOURCES += internetcontent/*.py internetcontent/nv_python_libs
    PYTHON_SOURCES += metadata/*

    python_pathfix.output  = $${OBJECTS_DIR}/${QMAKE_FILE_NAME}
    python_pathfix.commands = $${QMAKE_COPY_DIR} ${QMAKE_FILE_NAME} $${OBJECTS_DIR}/${QMAKE_FILE_NAME} $$escape_expand(\n\t) \
                              $${PYTHON} ./python_pathfix.py $${OBJECTS_DIR}/${QMAKE_FILE_NAME}
    python_pathfix.input = PYTHON_SOURCES
    python_pathfix.variable_out = PYTHON_FIXUPS
    python_pathfix.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += python_pathfix

    hardwareprofile_scripts.path = $${PREFIX}/share/mythtv/
    hardwareprofile_scripts.files = $${OBJECTS_DIR}/hardwareprofile
    hardwareprofile_scripts.CONFIG += no_check_exist

    metadata_scripts.path = $${PREFIX}/share/mythtv/
    metadata_scripts.files = $${OBJECTS_DIR}/metadata
    metadata_scripts.CONFIG += no_check_exists

    internetcontent_scripts.path = $${PREFIX}/share/mythtv/
    internetcontent_scripts.files = $${OBJECTS_DIR}/internetcontent
    internetcontent_scripts.CONFIG += no_check_exists

    INSTALLS += hardwareprofile_scripts metadata_scripts internetcontent_scripts

    unix|macx|mingw:QMAKE_CLEAN += -r $${OBJECTS_DIR}/hardwareprofile/distros \
                                      $${OBJECTS_DIR}/internetcontent/nv_python_libs \
                                      $${OBJECTS_DIR}/metadata/Movie \
                                      $${OBJECTS_DIR}/metadata/Music \
                                      $${OBJECTS_DIR}/metadata/Television
    unix|macx|mingw:QMAKE_DISTCLEAN += -r $${OBJECTS_DIR}/hardwareprofile \
                                          $${OBJECTS_DIR}/internetcontent \
                                          $${OBJECTS_DIR}/metadata
    win32-msvc*:QMAKE_CLEAN += /s /f /q $${OBJECTS_DIR}/hardwareprofile/distros \
                                        $${OBJECTS_DIR}/internetcontent/nv_python_libs \
                                        $${OBJECTS_DIR}/metadata/Movie \
                                        $${OBJECTS_DIR}/metadata/Music \
                                        $${OBJECTS_DIR}/metadata/Television $$escape_expand(\n\t) \
                               rd /s /q $${OBJECTS_DIR}/hardwareprofile/distros \
                                        $${OBJECTS_DIR}/internetcontent/nv_python_libs \
                                        $${OBJECTS_DIR}/metadata/Movie \
                                        $${OBJECTS_DIR}/metadata/Music \
                                        $${OBJECTS_DIR}/metadata/Television
    win32-msvc*:QMAKE_DISTCLEAN += /s /f /q $${OBJECTS_DIR}/*.* $$escape_expand(\n\t) \
                                   rd /s /q $${OBJECTS_DIR}/hardwareprofile \
                                            $${OBJECTS_DIR}/internetcontent \
                                            $${OBJECTS_DIR}/metadata

} else {

    # Try to verify our python exists
    win32 {
        PYTHON_BIN = $$system(where $${PYTHON})
    }
    unix|macx|mingw {
        PYTHON_BIN = $$system(which $${PYTHON})
    }

    PYTHON_SOURCES += metadata/Music/*

    python_pathfix.output  = $${OBJECTS_DIR}/${QMAKE_FILE_NAME}
    python_pathfix.commands = $${QMAKE_COPY_DIR} ${QMAKE_FILE_NAME} $${OBJECTS_DIR}/${QMAKE_FILE_NAME}
    !isEmpty(PYTHON_BIN) {
        python_pathfix.commands += $$escape_expand(\n\t)
        python_pathfix.commands += $${PYTHON} ./python_pathfix.py $${OBJECTS_DIR}/${QMAKE_FILE_NAME}
    }
    python_pathfix.input = PYTHON_SOURCES
    python_pathfix.variable_out = PYTHON_FIXUPS
    python_pathfix.CONFIG += no_link target_predeps
    QMAKE_EXTRA_COMPILERS += python_pathfix

    internetcontent_python_scripts.path = $${PREFIX}/share/mythtv/
    internetcontent_python_scripts.files = $${OBJECTS_DIR}/metadata
    internetcontent_python_scripts.CONFIG += no_check_exist

    INSTALLS += internetcontent_python_scripts

    unix|macx|mingw:QMAKE_CLEAN += -r $${OBJECTS_DIR}/metadata/Music/*
    unix|macx|mingw:QMAKE_DISTCLEAN += -r $${OBJECTS_DIR}/metadata
    win32-msvc*:QMAKE_CLEAN += /s /f /q $${OBJECTS_DIR}/metadata/Music/*.* $$escape_expand(\n\t) \
                               rd /s /q $${OBJECTS_DIR}/metadata/Music/*.*
    win32-msvc*:QMAKE_DISTCLEAN += /s /f /q $${OBJECTS_DIR}/metadata/*.* $$escape_expand(\n\t) \
                                   rd /s /q $${OBJECTS_DIR}/metadata


}

#
# internetcontent perl scripts do not require transformation
# but do require the perl bindings to function
#
using_bindings_perl {

    internetcontent_perl_scripts.path = $${PREFIX}/share/mythtv/internetcontent
    internetcontent_perl_scripts.files += internetcontent/nv_perl_libs internetcontent/*.pl

    INSTALLS += internetcontent_perl_scripts

}
