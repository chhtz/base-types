#include "spline.h"
#include "sisl/sisl.h"

#include <stdexcept>
#include <vector>

using namespace std;
using namespace base::geometry;
using namespace Eigen;

SplineBase::SplineBase (int dim, double _geometric_resolution, int _curve_order)
    : dimension(dim), curve(0), geometric_resolution(_geometric_resolution)
    , curve_order(_curve_order)
    , has_curve_length(false), curve_length(-1)
    , has_curvature_max(false), curvature_max(-1)
{
}

SplineBase::SplineBase(double geometric_resolution, SISLCurve* curve)
    : dimension(curve->idim), curve(curve), geometric_resolution(geometric_resolution)
    , curve_order(curve->ik)
    , has_curve_length(false), curve_length(-1)
    , has_curvature_max(false), curvature_max(-1)
{
    int status;

    s1363(curve, &start_param, &end_param, &status);
    if (status != 0)
        throw std::runtime_error("cannot get the curve start & end parameters");
}

SplineBase::~SplineBase ()
{
    // Frees the memory of the curve 
    if (curve)
        freeCurve(curve);
}

SplineBase::SplineBase(SplineBase const& source)
    : dimension(source.dimension)
    , curve(source.curve ? copyCurve(source.curve) : 0)
    , geometric_resolution(source.geometric_resolution)
    , curve_order(source.curve_order)
    , start_param(source.start_param)
    , end_param(source.end_param)
    , has_curve_length(source.has_curve_length), curve_length(source.curve_length)
    , has_curvature_max(source.has_curvature_max), curvature_max(source.curvature_max)
{
}

SplineBase const& SplineBase::operator = (SplineBase const& source)
{
    if (&source == this)
        return *this;

    dimension            = source.dimension;
    curve                = source.curve ? copyCurve(source.curve) : 0;
    geometric_resolution = source.geometric_resolution;
    curve_order          = source.curve_order;
    start_param          = source.start_param;
    end_param            = source.end_param;
    has_curve_length     = source.has_curve_length;
    curve_length         = source.curve_length;
    has_curvature_max    = source.has_curvature_max;
    curvature_max        = source.curvature_max;
    return *this;
}

int SplineBase::getPointCount() const
{ return curve->in; }

void SplineBase::getPoint(double* result, double _param)
{
    if (_param < start_param || _param > end_param) 
        throw std::out_of_range("_param is not in the [start_param, end_param] range");

    int leftknot; // Not needed
    int status;
    s1227(curve, 0, _param, &leftknot, result, &status); // Gets the point
    if (status != 0)
        throw std::runtime_error("SISL error while computing a curve point");
}

double SplineBase::getCurvature(double _param)
{
    // Limits the input paramter to the curve limit
    if (_param < start_param || _param > end_param) 
        throw std::out_of_range("_param is not in the [start_param, end_param] range");

    double curvature; 
    int status;
    s2550(curve, &_param, 1, &curvature, &status); // Gets the point
    if (status != 0)
        throw std::runtime_error("SISL error while computing a curvature");

    return curvature;
}

double SplineBase::getVariationOfCurvature(double _param)  // Variation of Curvature
{
    if (_param < start_param || _param > end_param) 
        throw std::out_of_range("_param is not in the [start_param, end_param] range");

    double VoC; 
    int status;
    s2556(curve, &_param, 1, &VoC, &status); // Gets the point
    if (status != 0)
        throw std::runtime_error("SISL error while computing a variation of curvature");

    return VoC;
}

double SplineBase::getCurveLength()
{
    if (has_curve_length)
        return curve_length;

    int status;
    s1240(curve, geometric_resolution, &curve_length, &status);
    if (status != 0)
        throw std::runtime_error("cannot get the curve length");

    has_curve_length = true;
    return curve_length;
}

double SplineBase::getUnitParameter()
{
    return (end_param - start_param) / getCurveLength();
}

double SplineBase::getCurvatureMax()
{
    if (has_curvature_max)
        return curvature_max;

    double const delPara = getUnitParameter() * geometric_resolution;
    curvature_max = 0.0;

    for (double p = start_param; p <= end_param; p+= delPara)
    {
	double curvature = getCurvature(p);
	if (curvature > curvature_max)
	    curvature_max = curvature;
    }

    has_curvature_max = true;
    return curvature_max;
}

bool SplineBase::isNURBS() const
{ return curve->ikind == 2 || curve->ikind == 4; }

void SplineBase::interpolate(std::vector<double> const& points, std::vector<double> const& parameters)
{
    vector<int> point_types;
    point_types.resize(points.size() / dimension, 1);

    // Generates curve
    double* point_param;  
    int nb_unique_param;
    start_param = 0.0;

    if (curve)
    {
        freeCurve(curve);
        curve = 0;
    }

    int status;
    if (parameters.empty())
    {
        s1356(const_cast<double*>(&points[0]), point_types.size(), dimension, &point_types[0],
                0, 0, 1, curve_order, start_param, &end_param, &curve, 
                &point_param, &nb_unique_param, &status);
    }
    else
    {
        s1357(const_cast<double*>(&points[0]), point_types.size(), dimension, &point_types[0],
                const_cast<double*>(&parameters[0]), 
                0, 0, 1, curve_order, start_param, &end_param, &curve, 
                &point_param, &nb_unique_param, &status);
    }
    free(point_param);
    if (status != 0)
        throw std::runtime_error("cannot generate the curve");

}

void SplineBase::printCurveProperties(std::ostream& io)
{
    io << "CURVE PROPERTIES " << std::endl
	<< "  Point count  : " << curve->in    << std::endl
	<< "  Order        : " << curve->ik    << std::endl
	<< "  Dimension    : " << curve->idim  << std::endl
	<< "  Kind         : " << curve->ikind << std::endl
	<< "  Parameters   : " << start_param  << "->" << end_param << std::endl
	<< "  Length       : " << getCurveLength() << std::endl;
}


double SplineBase::findOneClosestPoint(double const* _pt, double _geores)
{
    vector<double> points;
    vector< pair<double, double> > curves;
    findClosestPoints(_pt, points, curves, _geores);
    if (points.empty())
    {
        if (curves.empty())
            throw std::logic_error("no closes point returned by findClosestPoints");
        return curves.front().first;
    }
    else
        return points.front();
}

void SplineBase::findClosestPoints(double const* ref_point, vector<double>& _result_points, vector< pair<double, double> >& _result_curves, double _geores)
{
    int points_count;
    double* points;
    int curves_count;
    SISLIntcurve** curves;

    // Finds the closest point on the curve
    int status;
    s1953(curve, const_cast<double*>(ref_point), dimension, _geores, _geores, &points_count, &points, &curves_count, &curves, &status);
    if (status != 0)
        throw std::runtime_error("failed to find the closest points");

    for (int i = 0; i < curves_count; ++i)
        _result_curves.push_back(make_pair(curves[i]->epar1[0], curves[i]->epar1[1]));
    for (int i = 0; i < points_count; ++i)
        _result_points.push_back(points[i]);

    free(curves);
    free(points);
}

double SplineBase::localClosestPointSearch(double* ref_point, double _guess, double _start, double _end, double  _geores)
{
    double param;

    // Finds the closest point on the curve
    int status;
    s1774(curve, ref_point, dimension, _geores, _start, _end, _guess, &param, &status);
    if (status < 0)
        throw std::runtime_error("failed to find the closest points");

    // Returns the parameter of the point
    return param;
}



void SplineBase::clear()
{
    if (curve)
    {
        freeCurve(curve);
        curve = 0;
    }
}

vector<double> SplineBase::simplify()
{
    return simplify(geometric_resolution);
}

vector<double> SplineBase::simplify(double tolerance)
{
    if (!curve)
        throw std::runtime_error("the curve is not initialized");

    SISLCurve* result = NULL;
    double epsilon[3] = { tolerance, tolerance, tolerance };

    double maxerr[3];
    int status;
    s1940(curve, epsilon,
            curve_order, // derivatives
            curve_order, // derivatives
            1, // request closed curve
            10, // number of iterations
            &result, maxerr, &status);
    if (status != 0)
        throw std::runtime_error("SISL error while simplifying a curve");

    freeCurve(curve);
    curve = result;
    return vector<double>(maxerr, maxerr + 3);
}

SISLCurve const* SplineBase::getSISLCurve() const
{
    return curve;
}

SISLCurve* SplineBase::getSISLCurve()
{
    return curve;
}

Eigen::Matrix3d SplineBase::getFrenetFrame(double _param)
{
    double p;    // does nothing
    double t[3], n[3], b[3]; // Frame axis

    // Finds the frenet frame
    int status;
    s2559(curve, &_param, 1, &p, t, n, b, &status);

    // Writes the frame to a matrix
    Matrix3d frame;
    frame << t[0], t[1], t[2], n[0], n[1], n[2], b[0], b[1], b[2];

    return frame;
}

double SplineBase::getHeading(double _param)
{    
    Matrix3d frame = getFrenetFrame(_param);

    // Vector if the X axis of the frame
    Vector2d Xaxis(frame(0,0),frame(0,1));
    Xaxis.normalize(); 

    // Returns the angle of Frenet X axis in Inertial frame
    return atan2(Xaxis.y(),Xaxis.x());
}

double SplineBase::headingError(double _actHeading, double _param)
{
    // Orientation error
    double error = _actHeading - getHeading(_param);
    if(error > M_PI)
	return error - 2*M_PI;
    else if (error < -M_PI)
	return error + 2*M_PI;
    else
     	return error;
}

double SplineBase::distanceError(Eigen::Vector3d _pt, double _param)
{
    // Error vector
    Vector3d curve_point;
    getPoint(curve_point.data(), _param);
    Vector3d error = _pt - curve_point;
    error(2) = 0.0;  // Z axis error not needed

    // Finds the angle of error vector to the Frenet X axis 
    Vector2d pt_vec(error(0),error(1));
    pt_vec.normalize(); 
    double  angle = atan2(pt_vec.y(),pt_vec.x()) - getHeading(_param);

    // Sign of the distance error depending on position of the 
    // actual robot in Frenet frame
    return (angle >= 0.0)?(error.norm()):(-error.norm());
}

Eigen::Vector3d SplineBase::poseError(Eigen::Vector3d _pt, double _actZRot, double _st_para, double _len_tol)
{
    // Finds the search length
    double del_para = getUnitParameter() *  _len_tol;

    // Finds teh closest poiont in the search length
    double param = localClosestPointSearch(_pt.data(), _st_para, _st_para, _st_para + del_para, getGeometricResolution());

    // Returns the error [distance error, orientation error, parameter] 
    return Eigen::Vector3d(distanceError(_pt, param), headingError(_actZRot, param), param);
}



