#include <sstream>
#include <string>

#include <vapor/SliceRenderer.h>
#include <vapor/SliceParams.h>
#include <vapor/ControlExecutive.h>
#include <vapor/LegacyGL.h>
#include <vapor/GLManager.h>
#include <vapor/GetAppPath.h>

#define X 0
#define Y 1 
#define Z 2
#define XY 0
#define XZ 1
#define YZ 2
#define NUMVERTICES 4

#define MAXTEXTURESIZE 8000

bool useEBO = false;

using namespace VAPoR;

static RendererRegistrar<SliceRenderer> registrar(
    SliceRenderer::GetClassType(), SliceParams::GetClassType()
);

SliceRenderer::SliceRenderer(
    const ParamsMgr* pm,
    string winName,
    string dataSetName,
    string instanceName,
    DataMgr* dataMgr
) : Renderer(
    pm, 
    winName, 
    dataSetName, 
    SliceParams::GetClassType(),
    SliceRenderer::GetClassType(), 
    instanceName, 
    dataMgr
) {
    _initialized   = false;
    _textureWidth  = 250;
    _textureHeight = 250;

    _texCoords = {
        0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f
    };

    //_VAO        = 0;
    _EBO        = 0;

    _cacheParams.domainMin.resize(3, 0.f);
    _cacheParams.domainMax.resize(3, 1.f);

    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);
    MapperFunction* tf = p->GetMapperFunc(_cacheParams.varName);

    _colorMapSize = tf->getNumEntries();

    _dataValues = new float[_textureWidth * _textureHeight];
    
    _vertexCoords.clear();

    _initialized = true;
}

SliceRenderer::~SliceRenderer() {
    if (_dataValues) {
        delete [] _dataValues;
        _dataValues = nullptr;
    }

    glDeleteVertexArrays(1, &_VAO);
    glDeleteBuffers(1, &_vertexVBO);
    glDeleteBuffers(1, &_texCoordVBO);
    glDeleteBuffers(1, &_EBO);
    
    glDeleteTextures(1, &_colorMapTextureID);
    glDeleteTextures(1, &_dataValueTextureID);
}

int SliceRenderer::_initializeGL() {
    _resetDataCache();
    
    _initVAO();
    _initTexCoordVBO();
    _initVertexVBO();

    return 0;
}

void SliceRenderer::_initVAO() {
    glGenVertexArrays(1, &_VAO);
    glBindVertexArray(_VAO);
}

void SliceRenderer::_initTexCoordVBO() {
    glDeleteBuffers(1, &_texCoordVBO);
    glGenBuffers(1, &_texCoordVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _texCoordVBO);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(1);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(float)*sizeof(_texCoords),
        &_texCoords[0],
        GL_STATIC_DRAW
    );
}

void SliceRenderer::_initVertexVBO() {
    glDeleteBuffers(1, &_vertexVBO);
    glGenBuffers(1, &_vertexVBO);
    glBindBuffer(GL_ARRAY_BUFFER, _vertexVBO);
    glVertexAttribPointer(0, 3, GL_DOUBLE, GL_FALSE, 0, (void*)0);
    glEnableVertexAttribArray(0);
    if (useEBO) {
        glBufferData(
            GL_ARRAY_BUFFER, 
            NUMVERTICES*3*sizeof(double), 
            &_vertexCoords[0], 
            GL_STATIC_DRAW
        );
    }
    else {
        glBufferData(
            GL_ARRAY_BUFFER, 
            6*3*sizeof(double), 
            &_vertexCoords[0], 
            GL_STATIC_DRAW
        );
    }
}

int SliceRenderer::_resetDataCache() {
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);

    _cacheParams.varName            = p->GetVariableName();
    _cacheParams.heightVarName      = p->GetHeightVariableName();
    _cacheParams.ts                 = p->GetCurrentTimestep();
    _cacheParams.refinementLevel    = p->GetRefinementLevel();
    _cacheParams.compressionLevel   = p->GetCompressionLevel();
    _cacheParams.textureSampleRate  = p->GetSampleRate();
    _cacheParams.orientation        = p->GetBox()->GetOrientation();

    _textureWidth  = _cacheParams.textureSampleRate;
    _textureHeight = _cacheParams.textureSampleRate;
    if (_textureWidth > MAXTEXTURESIZE)
        _textureWidth = MAXTEXTURESIZE;
    if (_textureHeight > MAXTEXTURESIZE)
        _textureHeight = MAXTEXTURESIZE;
 
/*    p->GetBox()->GetExtents(_cacheParams.boxMin, _cacheParams.boxMax);

    rc = _dataMgr->GetVariableExtents(
        _cacheParams.ts,
        _cacheParams.varName,
        _cacheParams.refinementLevel,
        _cacheParams.domainMin,
        _cacheParams.domainMax
    );
    if (!rc) {
        SetErrMsg("
            Unable to determine domain extents for %s", 
            _cacheParams.varName
        );
        return rc;
    }
    _resetTextureCoordinates();
*/
   
    _resetBoxCache();
    _resetColormapCache();
 
    if (_dataValues) delete [] _dataValues;
    _dataValues = new float[_textureWidth * _textureHeight];

    int rc = _saveTextureData();
    if (rc<0) {
        SetErrMsg("Unable to acquire data for Slice texture");
        return rc;
    }
    
    cout << _dataValues[0] << endl;
    cout << _cacheParams.tf_lut[0] << endl;

    glDeleteTextures(1, &_dataValueTextureID);
    glGenTextures(1, &_dataValueTextureID);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _dataValueTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

//    for (int i=0; i<_textureWidth*_textureHeight; i++ ) {
//        cout << _dataValues[i] << " ";
//    }
//    cout << endl;

    glTexImage2D(
        GL_TEXTURE_2D, 
        0, 
        GL_R32F, 
        _textureWidth,
        _textureHeight,
        0, 
        GL_RED, 
        GL_FLOAT, 
        _dataValues
    );

    return rc;
}

void SliceRenderer::_resetColormapCache() {
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);

    MapperFunction* tf = p->GetMapperFunc(_cacheParams.varName);
    tf->makeLut(_cacheParams.tf_lut);
    _cacheParams.tf_minMax = tf->getMinMaxMapValue();

//    for (int i=0; i<256; i++ ) {
//        cout << _cacheParams.tf_lut[i] << " ";
//    }
//    cout << endl;

    glDeleteTextures(1, &_colorMapTextureID);
    glGenTextures(1, &_colorMapTextureID);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, _colorMapTextureID);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage1D(
        GL_TEXTURE_1D, 
        0, 
        GL_RGBA8, 
        _colorMapSize,
        0, 
        GL_RGBA, 
        GL_FLOAT, 
        &_cacheParams.tf_lut[0]
    );
}
   
int SliceRenderer::_resetBoxCache() { 
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);
    p->GetBox()->GetExtents(_cacheParams.boxMin, _cacheParams.boxMax);

    int rc = _dataMgr->GetVariableExtents(
        _cacheParams.ts,
        _cacheParams.varName,
        _cacheParams.refinementLevel,
        _cacheParams.domainMin,
        _cacheParams.domainMax
    );
    if (rc < 0) {
        SetErrMsg(
            "Unable to determine domain extents for %s", 
            _cacheParams.varName.c_str()
        );
        return rc;
    }
    _setVertexPositions();
    _resetTextureCoordinates();
    return rc;
}

void SliceRenderer::_resetTextureCoordinates() {
    float texMinX, texMinY, texMaxX, texMaxY;
    
    std::vector<double> boxMin = _cacheParams.boxMin;
    std::vector<double> boxMax = _cacheParams.boxMax;
    std::vector<double> domainMin = _cacheParams.domainMin;
    std::vector<double> domainMax = _cacheParams.domainMax;

    int xAxis, yAxis;
    int orientation = _cacheParams.orientation;
    if (orientation == XY) {
        xAxis = X;
        yAxis = Y;
    }
    else if (orientation == XZ) {
        xAxis = X;
        yAxis = Z;
    }
    else {   // (orientation = YZ)
        xAxis = Y;
        yAxis = Z;
    }

    texMinX = ( boxMin[xAxis] - domainMin[xAxis] ) / 
              ( domainMax[xAxis] - domainMin[xAxis] );
    texMaxX = ( boxMax[xAxis] - domainMin[xAxis] ) / 
              ( domainMax[xAxis] - domainMin[xAxis] );
    texMinY = ( boxMin[yAxis] - domainMin[yAxis] ) / 
              ( domainMax[yAxis] - domainMin[yAxis] );
    texMaxY = ( boxMax[yAxis] - domainMin[yAxis] ) / 
              ( domainMax[yAxis] - domainMin[yAxis] );

    //cout << endl;
    //for (int i=0; i<6; i++)
    //    cout << _texCoords[i*2] << " " << _texCoords[i*2+1] << endl;
    _texCoords.clear();
    _texCoords = {
        texMinX, texMinY,
        texMaxX, texMinY,
        texMinX, texMaxY,
        texMaxX, texMinY,
        texMaxX, texMaxY,
        texMinX, texMaxY
        /*0.0f, 0.0f,
        1.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 1.0f*/
    };
    cout << endl;
    for (int i=0; i<6; i++)
        cout << _texCoords[i*2] << " " << _texCoords[i*2+1] << endl;
}

void SliceRenderer::_getSampleCoordinates(
    std::vector<double> &coords, 
    int i, 
    int j
) const {
    int sampleRate = _cacheParams.textureSampleRate;
    double dx = (_cacheParams.domainMax[X]-_cacheParams.domainMin[X])/(1+sampleRate);
    double dy = (_cacheParams.domainMax[Y]-_cacheParams.domainMin[Y])/(1+sampleRate);
    double dz = (_cacheParams.domainMax[Z]-_cacheParams.domainMin[Z])/(1+sampleRate);

    if (_cacheParams.orientation == XY) {
        coords[X] = _cacheParams.domainMin[X] + dx*i + dx/2.f;
        coords[Y] = _cacheParams.domainMin[Y] + dy*j + dy/2.f;
        coords[Z] = _cacheParams.domainMin[Z];
    }
    else if (_cacheParams.orientation == XZ) {
        coords[X] = _cacheParams.domainMin[X] + dx*i + dx/2.f;
        coords[Y] = _cacheParams.domainMin[Y];
        coords[Z] = _cacheParams.domainMin[Z] + dz*j + dz/2.f;
    }
    else {  // Y corresponds to i, the faster axis; Z to j, the slower axis
        coords[Z] = _cacheParams.domainMin[Z] + dz*j + dz/2.f;
        coords[Y] = _cacheParams.domainMin[Y] + dy*i + dy/2.f;
        coords[X] = _cacheParams.domainMin[X];
    }
}

/*void SliceRenderer::_getSampleCoordinates(
    std::vector<double> &coords, 
    int i, 
    int j
) const {
    int sampleRate = _cacheParams.textureSampleRate;
    double dx = (_cacheParams.boxMax[X]-_cacheParams.boxMin[X])/(1+sampleRate);
    double dy = (_cacheParams.boxMax[Y]-_cacheParams.boxMin[Y])/(1+sampleRate);
    double dz = (_cacheParams.boxMax[Z]-_cacheParams.boxMin[Z])/(1+sampleRate);

    if (_cacheParams.orientation == XY) {
        coords[X] = _cacheParams.boxMin[X] + dx*i + dx/2.f;
        coords[Y] = _cacheParams.boxMin[Y] + dy*j + dy/2.f;
        coords[Z] = _cacheParams.boxMin[Z];
    }
    else if (_cacheParams.orientation == XZ) {
        coords[X] = _cacheParams.boxMin[X] + dx*i + dx/2.f;
        coords[Y] = _cacheParams.boxMin[Y];
        coords[Z] = _cacheParams.boxMin[Z] + dz*j + dz/2.f;
    }
    else {  // Y corresponds to i, the faster axis; Z to j, the slower axis
        coords[Z] = _cacheParams.boxMin[Z] + dz*j + dz/2.f;
        coords[Y] = _cacheParams.boxMin[Y] + dy*i + dy/2.f;
        coords[X] = _cacheParams.boxMin[X];
    }
}*/

int SliceRenderer::_saveTextureData() {
    Grid* grid = NULL;
    int rc = DataMgrUtils::GetGrids(
        _dataMgr, 
        _cacheParams.ts,
        _cacheParams.varName,
        _cacheParams.boxMin,
        _cacheParams.boxMax,
        true,
        &_cacheParams.refinementLevel,
        &_cacheParams.compressionLevel,
        &grid
    );

    grid->SetInterpolationOrder(1);
    
    if (rc<0) {
        SetErrMsg("Unable to acquire Grid for Slice texture");
        return(rc);
    }
    assert(grid);

    _setVertexPositions();

    float varValue, missingValue; 
    std::vector<double> coords(3, 0.0); 
    for (int j=0; j<_textureHeight; j++) {
        for (int i=0; i<_textureWidth; i++) {
            _getSampleCoordinates(coords, i, j);

            int index = (j*_textureWidth + i);

            varValue = grid->GetValue(coords);
            missingValue = grid->GetMissingValue();
            if (varValue == missingValue) {
                _dataValues[index] = NAN;
                continue;
            }

            _dataValues[index]   = varValue;
        }
    }
    return rc;
}

bool SliceRenderer::_isDataCacheDirty() const
{
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);

    if (_cacheParams.varName          != p->GetVariableName())       return true;
    if (_cacheParams.heightVarName    != p->GetHeightVariableName()) return true;
    if (_cacheParams.ts               != p->GetCurrentTimestep())    return true;
    if (_cacheParams.refinementLevel  != p->GetRefinementLevel())    return true;
    if (_cacheParams.compressionLevel != p->GetCompressionLevel())   return true;

    if (_cacheParams.textureSampleRate != p->GetSampleRate())        return true;

    vector<double> min, max;
    Box* box = p->GetBox();
    int orientation = box->GetOrientation();
    if (_cacheParams.orientation      != orientation)                return true;
    // Special case: if our plane shifts its position along its orthognal axis,
    // then we will need to return true and resample our data.  If its extents 
    // change along its perimeter, then we will just reconfigure the texture 
    // coordinates via _resetBoxCache in our _paintGL routine.
    box->GetExtents(min, max);
    int constantAxis = _getConstantAxis();
    if (min[constantAxis] != _cacheParams.boxMin[constantAxis])      return true;

//    if (_cacheParams.boxMin != min)                                  return true;
//    if (_cacheParams.boxMax != max)                                  return true;

    return false;
}

bool SliceRenderer::_isColormapCacheDirty() const {
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);

    MapperFunction *tf = p->GetMapperFunc(_cacheParams.varName);
    vector <float> tf_lut;
    tf->makeLut(tf_lut);
    if (_cacheParams.tf_lut != tf_lut)                               return true;
    if (_cacheParams.tf_minMax != tf->getMinMaxMapValue())           return true;
    return false;
}

bool SliceRenderer::_isBoxCacheDirty() const {
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);

    Box* box = p->GetBox();
    vector<double> min, max;
    box->GetExtents(min, max);
    if (_cacheParams.boxMin != min)                                  return true;
    if (_cacheParams.boxMax != max)                                  return true;
    return false;
}

int SliceRenderer::_paintGL(bool fast) {
    int rc = 0;

    _initializeState();

    if (_isDataCacheDirty()) {
        //_resetColormapCache();
        rc = _resetDataCache();
    }
    else if (_isColormapCacheDirty()) {
        _resetColormapCache();
    }
    else if (_isBoxCacheDirty()) {
        rc = _resetBoxCache();
    }

    if (rc<0)
        return rc;  // error message set by _resetBoxCache()

    if (rc<0)
        return rc;  // error message set by _resetDataCache()

    if (useEBO) {
        glGenBuffers(1, &_EBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _EBO);
        unsigned short indexValues [] = { 0, 1, 2, 1, 3, 2  };
        glBufferData(
            GL_ELEMENT_ARRAY_BUFFER, 
            //indexValues.size()*sizeof(unsigned short), 
            sizeof(indexValues),
            //&indexValues[0], 
            &indexValues,
            GL_STATIC_DRAW
        );
    }

    _configureShader();

    if (printOpenGLError() !=0) {
        return -1;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, _colorMapTextureID);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, _dataValueTextureID);

    glBindVertexArray(_VAO);
    if (useEBO) {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, _EBO);
        //glDrawElements(GL_TRIANGLES, 2, GL_UNSIGNED_INT, 0); 
        glDrawArrays(GL_TRIANGLES, 0, 4);
    }
    else {
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    _resetState();

    if (printOpenGLError() !=0) {
        return -1;
    }
    return rc;
}

void SliceRenderer::_configureShader() {
    ShaderProgram* s = _glManager->shaderManager->GetShader("Slice");
//    EnableClipToBox(s);
    s->Bind();
    
    // One vertex shader uniform vec4
    s->SetUniform("MVP", _glManager->matrixManager->GetModelViewProjectionMatrix());

    // Remaining fragment shader uniform floats
    SliceParams* p = dynamic_cast<SliceParams*>(GetActiveParams());
    assert(p);
    float opacity = p->GetConstantOpacity();
    s->SetUniform("constantOpacity", opacity);
    s->SetUniform("minLUTValue", (float)_cacheParams.tf_minMax[0]);
    s->SetUniform("maxLUTValue", (float)_cacheParams.tf_minMax[1]);

    // And finally our uniform samplers
    GLint colormapLocation;
    colormapLocation = s->GetUniformLocation("colormap");
    glUniform1i(colormapLocation, 0);

    GLint dataValuesLocation;
    dataValuesLocation = s->GetUniformLocation("dataValues");
    glUniform1i(dataValuesLocation, 1);
}

void SliceRenderer::_initializeState() {
    _glManager->matrixManager->MatrixModeModelView();
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
}

void SliceRenderer::_resetState() {
    ShaderProgram* s = _glManager->shaderManager->GetShader("Slice");
    s->UnBind();
    
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_1D, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);
   
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
}

void SliceRenderer::_setVertexPositions() {
    std::vector<double> min = _cacheParams.boxMin;
    std::vector<double> max = _cacheParams.boxMax;
    int orientation = _cacheParams.orientation;
    if (orientation == XY) {
        _setXYVertexPositions(min, max);
    }
    else if (orientation == XZ)
        _setXZVertexPositions(min, max);
    else if (orientation == YZ)
        _setYZVertexPositions(min, max);

    for (int i=0; i<_vertexCoords.size(); i++) {
        if ((i)%3==0) cout << endl << (i)/3 << " ";
        cout << _vertexCoords[i] << " ";
    }
    cout << endl;
}

void SliceRenderer::_setXYVertexPositions(
    std::vector<double> min, 
    std::vector<double> max
) {
    double zCoord = min[Z];

    std::vector<double> temp;
    if (useEBO) {
        std::vector<double> temp = {
            min[X], min[Y], zCoord,
            max[X], min[Y], zCoord,
            max[X], max[Y], zCoord,
            min[X], max[Y], zCoord
        };
        _vertexCoords = temp;
    }
    else {
        std::vector<double> temp = {
            min[X], min[Y], zCoord,
            max[X], min[Y], zCoord,
            min[X], max[Y], zCoord,
            max[X], min[Y], zCoord,
            max[X], max[Y], zCoord,
            min[X], max[Y], zCoord
        };
        _vertexCoords = temp;
    }
}

void SliceRenderer::_setXZVertexPositions(
    std::vector<double> min, 
    std::vector<double> max
) {
    double yCoord = min[Y];
    std::vector<double> temp = {
        min[X], yCoord, min[Z],
        max[X], yCoord, min[Z],
        min[X], yCoord, max[Z],
        max[X], yCoord, min[Z],
        max[X], yCoord, max[Z],
        min[X], yCoord, max[Z]
    };

/*    std::vector<double> temp = {
        min[X], yCoord, min[Z],
        max[X], yCoord, min[Z],
        max[X], yCoord, max[Z],
        min[X], yCoord, max[Z]
    };*/

    _vertexCoords = temp;
}

void SliceRenderer::_setYZVertexPositions(
    std::vector<double> min, 
    std::vector<double> max
) {
    double xCoord = min[X];
    std::vector<double> temp = {
        xCoord, min[Y], min[Z],
        xCoord, max[Y], min[Z],
        xCoord, min[Y], max[Z],
        xCoord, max[Y], min[Z],
        xCoord, max[Y], max[Z],
        xCoord, min[Y], max[Z]
    };

/*    std::vector<double> temp = {
        xCoord, min[Y], min[Z],
        xCoord, max[Y], min[Z],
        xCoord, max[Y], max[Z],
        xCoord, min[Y], max[Z]
    };*/

    _vertexCoords = temp;
}

int SliceRenderer::_getConstantAxis() const {
    if (_cacheParams.orientation == XY )
        return Z;
    else if (_cacheParams.orientation == XZ )
        return Y;
    else // (_cacheParams.orientation == XY )
        return X;
}

