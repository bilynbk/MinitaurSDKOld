
#include <stdio.h>
#include <SDK.h>
#include <math.h>
#include <Motor.h>
#include <ReorientableBehavior.h>
#include "Interpolator.h"

#if defined(ROBOT_MINITAUR)
// Subject to change for individual robots
// const float motZeros[8] = {2.82, 3.435, 3.54, 3.076, 1.03, 3.08, 6.190, 1.493};
// const float motZeros[8] = {0.93, 5.712, 3.777, 3.853, 2.183, 1.556, .675, 2.679}; // RML Ellie
const float motZeros[9] = {0.93, 5.712, 3.777, 3.853, 2.183, 1.556, .675, 2.679, 2.906}; // RML Ellie with G4 tail
// const float motZeros[8] = {0.631, 4.076, 1.852, 3.414, 1.817, 5.500, 1.078, 6.252}; //RML Odie
#endif

#define TIMESTEPS 42
#define MOTORS 9
#define TAIL_MOT 8

enum TAMode
{
	TA_SIT = 0,
	TA_STAND,
	TA_GO
};

char myData[32];
float* myData_buf = (float*)myData;

// globals for power monitoring values
float voltage, current;
float oldvoltage, oldcurrent;
float power_int = 0;

#pragma pack(push, 1)
// We receive the serial protocol version number (in case we add more fields later)
// our behaviorCmd, and a checksum.
struct SerialCommandPacket
{
	float voltage, current;
};

const char ALIGNMENT_WORD[2] = {'G', 'R'};

#pragma pack(pop)

// Ground speed matching and impact minimization - energyOptimalBoundInstantaneousSwitchGSMClearance.mat
float times[TIMESTEPS] = {0.000000,0.008550,0.017099,0.025649,0.034199,0.042748,0.051298,0.059847,0.068397,0.076947,0.085496,0.094046,0.102596,0.111145,0.119695,0.128244,0.136794,0.145344,0.153893,0.162443,0.170993,0.170993,0.171283,0.171573,0.171863,0.172153,0.172443,0.172733,0.173023,0.173313,0.173603,0.173894,0.174184,0.174474,0.174764,0.175054,0.175344,0.175634,0.175924,0.176214,0.176504,0.176795};
float pos[TIMESTEPS][MOTORS] = {{1.570696,1.570696,1.570896,1.570896,1.570696,1.570696,1.570896,1.570896,1.473079},{1.564419,1.562039,1.570236,1.567853,1.562041,1.564419,1.567853,1.570236,1.467662},{1.546583,1.537731,1.566937,1.558200,1.537731,1.546583,1.558200,1.566937,1.452047},{1.517091,1.498589,1.560465,1.542320,1.498589,1.517091,1.542321,1.560465,1.426198},{1.477554,1.447583,1.548133,1.519683,1.447583,1.477553,1.519684,1.548133,1.391424},{1.430342,1.391557,1.526414,1.490402,1.391549,1.430342,1.490404,1.526417,1.348920},{1.376556,1.332429,1.492926,1.453601,1.332412,1.376556,1.453603,1.492927,1.299745},{1.315047,1.270059,1.447399,1.409055,1.270042,1.315047,1.409059,1.447401,1.243503},{1.244125,1.201143,1.388996,1.355348,1.201127,1.244125,1.355352,1.388998,1.180308},{1.161766,1.124041,1.317106,1.291386,1.124026,1.161766,1.291392,1.317109,1.109560},{1.059223,1.024305,1.232556,1.215010,1.024290,1.059223,1.215017,1.232560,1.030296},{0.906393,0.856117,1.142507,1.125644,0.856101,0.906394,1.125654,1.142512,0.938413},{0.696299,0.613272,1.036564,1.019036,0.613256,0.696299,1.019048,1.036571,0.837784},{0.475867,0.375781,0.882305,0.879458,0.375767,0.475870,0.879474,0.882314,0.738335},{0.333648,0.266354,0.679398,0.705282,0.266345,0.333657,0.705304,0.679413,0.640766},{0.336128,0.364903,0.482289,0.548909,0.364901,0.336140,0.548937,0.482308,0.538788},{0.467105,0.641080,0.327233,0.462464,0.641085,0.467116,0.462495,0.327253,0.432731},{0.663342,1.004342,0.176920,0.423081,1.004347,0.663348,0.423101,0.176926,0.329450},{0.859134,1.352646,0.094067,0.505943,1.352651,0.859137,0.505942,0.094058,0.225006},{1.029855,1.638287,0.210920,0.830648,1.638272,1.029855,0.830637,0.210899,0.106933},{1.186873,1.871546,0.470819,1.259286,1.871550,1.186868,1.259279,0.470797,-0.036499},{1.186773,1.871446,0.470919,1.259386,1.871451,1.186768,1.259379,0.470897,-0.036399},{1.191956,1.878195,0.481034,1.273366,1.878199,1.191951,1.273360,0.481013,-0.041645},{1.196700,1.884751,0.491028,1.287440,1.884755,1.196695,1.287433,0.491007,-0.047129},{1.201358,1.890757,0.501286,1.301217,1.890761,1.201353,1.301210,0.501264,-0.052475},{1.205547,1.896569,0.511411,1.315081,1.896573,1.205542,1.315074,0.511389,-0.058059},{1.209650,1.901836,0.521791,1.328640,1.901839,1.209645,1.328634,0.521770,-0.063503},{1.213295,1.906915,0.532030,1.342280,1.906919,1.213290,1.342274,0.532009,-0.069185},{1.216862,1.911456,0.542517,1.355611,1.911460,1.216857,1.355604,0.542495,-0.074724},{1.219978,1.915815,0.552855,1.369017,1.915819,1.219973,1.369010,0.552834,-0.080501},{1.223023,1.919643,0.563433,1.382111,1.919647,1.223018,1.382104,0.563412,-0.086134},{1.225620,1.923293,0.573858,1.395277,1.923297,1.225615,1.395270,0.573837,-0.092003},{1.228150,1.926416,0.584516,1.408128,1.926419,1.228145,1.408121,0.584495,-0.097727},{1.230236,1.929364,0.595016,1.421049,1.929368,1.230231,1.421043,0.594994,-0.103686},{1.232254,1.931789,0.605744,1.433655,1.931793,1.232249,1.433648,0.605723,-0.109499},{1.233828,1.934042,0.616309,1.446330,1.934046,1.233824,1.446323,0.616288,-0.115546},{1.235334,1.935774,0.627098,1.458688,1.935778,1.235329,1.458681,0.627077,-0.121447},{1.236394,1.937336,0.637720,1.471115,1.937340,1.236389,1.471109,0.637699,-0.127580},{1.237382,1.938379,0.648563,1.483227,1.938382,1.237377,1.483220,0.648542,-0.133566},{1.237921,1.939252,0.659237,1.495408,1.939255,1.237916,1.495401,0.659216,-0.139784},{1.238360,1.939604,0.670136,1.507281,1.939608,1.238356,1.507275,0.670115,-0.145857},{1.238385,1.939802,0.680843,1.519213,1.939806,1.238381,1.519207,0.680823,-0.152158}};
float vel[TIMESTEPS][MOTORS] = {{-0.000100,-0.000099,0.000099,0.000100,-0.000099,-0.000100,0.000100,0.000099,-0.000099},{-1.418803,-1.957029,-0.213404,-0.747671,-1.957027,-1.418807,-0.747657,-0.213397,-1.245555},{-2.750840,-3.704102,-0.570830,-1.499574,-3.704097,-2.750847,-1.499549,-0.570818,-2.432566},{-4.072059,-5.364932,-1.041720,-2.254194,-5.364922,-4.072070,-2.254153,-1.041701,-3.576945},{-5.147627,-6.433522,-1.895330,-3.033983,-6.433503,-5.147639,-3.033928,-1.895306,-4.566942},{-5.881111,-6.708773,-3.226898,-3.860248,-6.708737,-5.881115,-3.860175,-3.226866,-5.365848},{-6.731984,-7.112739,-4.603724,-4.747701,-7.112692,-6.731981,-4.747610,-4.603685,-6.174217},{-7.680396,-7.597916,-6.082342,-5.729524,-7.597861,-7.680384,-5.729411,-6.082293,-6.980808},{-8.980552,-8.597209,-7.570156,-6.845507,-8.597147,-8.980532,-6.845369,-7.570095,-7.847119},{-10.529109,-9.910832,-9.218190,-8.182388,-9.910762,-10.529080,-8.182217,-9.218109,-8.735115},{-13.748816,-13.845359,-10.485517,-9.703600,-13.845301,-13.748787,-9.703387,-10.485411,-9.885614},{-21.593050,-24.783232,-11.041066,-11.351516,-24.783244,-21.593065,-11.351245,-11.040926,-11.430802},{-27.191141,-31.284871,-14.157031,-13.691100,-31.284891,-27.191130,-13.690744,-14.156839,-11.977240},{-22.794114,-22.300099,-21.405083,-18.674959,-22.299672,-22.793488,-18.674392,-21.404721,-11.424706},{-8.848878,-1.280301,-25.583534,-21.737959,-1.279789,-8.848068,-21.737178,-25.582975,-11.491177},{8.636760,23.120553,-20.557706,-14.541833,23.120771,8.636765,-14.541159,-20.557263,-12.285714},{21.166144,40.318883,-15.792886,-5.333059,40.319009,21.165651,-5.333086,-15.793085,-12.398973},{23.830280,43.133200,-16.523141,-0.665610,43.133253,23.829833,-0.667818,-16.525122,-11.975262},{21.107862,36.866418,0.034474,23.217023,36.866415,21.107487,23.214616,0.032143,-12.624309},{19.017981,30.166851,24.669713,48.378455,30.166760,19.017484,48.378365,24.669156,-15.165241},{17.856294,24.579574,33.450880,47.576505,24.579489,17.855775,47.577139,33.451092,-18.509726},{17.786675,24.374484,34.231846,48.860865,24.374480,17.786683,48.860947,34.231925,-18.490936},{16.947847,23.103007,34.491202,48.527073,23.103007,16.947848,48.527092,34.491219,-18.665054},{16.047798,21.825159,34.733931,48.177517,21.825161,16.047801,48.177547,34.733955,-18.838166},{15.073441,20.536815,34.961204,47.813639,20.536815,15.073441,47.813646,34.961210,-19.010332},{14.117650,19.261931,35.172770,47.437988,19.261933,14.117651,47.437998,35.172779,-19.180219},{13.180526,18.000058,35.369439,47.051896,18.000058,13.180526,47.051898,35.369441,-19.347649},{12.256763,16.749303,35.552625,46.656979,16.749304,12.256764,46.656982,35.552629,-19.512947},{11.346472,15.509248,35.723050,46.254384,15.509248,11.346472,46.254384,35.723051,-19.675950},{10.445061,14.278199,35.881949,45.845482,14.278200,10.445062,45.845483,35.881951,-19.836966},{9.552585,13.055747,36.029962,45.431259,13.055747,9.552585,45.431259,36.029963,-19.995840},{8.665441,11.840478,36.168158,45.012872,11.840479,8.665441,45.012871,36.168160,-20.152862},{7.783986,10.632088,36.297097,44.591166,10.632088,7.783986,44.591165,36.297097,-20.307876},{6.902915,9.428713,36.417747,44.167091,9.428713,6.902915,44.167090,36.417747,-20.461199},{6.022296,8.229989,36.530604,43.741362,8.229989,6.022297,43.741362,36.530604,-20.612684},{5.139181,7.034705,36.636479,43.314783,7.034705,5.139181,43.314782,36.636479,-20.762602},{4.253726,5.842539,36.735804,42.887958,5.842539,4.253724,42.887958,36.735804,-20.910809},{3.362519,4.652181,36.829298,42.461543,4.652180,3.362517,42.461542,36.829299,-21.057579},{2.465574,3.463284,36.917344,42.036048,3.463282,2.465580,42.036048,36.917344,-21.202774},{1.560527,2.274849,37.000519,41.611991,2.274854,1.560531,41.611990,37.000519,-21.346632},{0.578368,1.060896,37.113395,41.208766,1.060913,0.578342,41.208766,37.113395,-21.496505},{-0.000092,0.000080,37.014477,40.690713,0.000076,-0.000092,40.690708,37.014476,-21.600288}};
float u[TIMESTEPS][MOTORS] = {{-0.030509,0.173642,-0.082392,-0.083586,0.173728,-0.030492,-0.083587,-0.082392,-2.999997},{-0.030509,0.173642,-0.082392,-0.083586,0.173728,-0.030492,-0.083587,-0.082392,-2.999997},{0.050893,0.559282,-0.083421,-0.085758,0.559405,0.050913,-0.085758,-0.083421,-2.999998},{0.050893,0.559282,-0.083421,-0.085758,0.559405,0.050913,-0.085758,-0.083421,-2.999998},{0.224285,1.505066,-0.087041,-0.092538,1.505226,0.224300,-0.092539,-0.087041,-2.999998},{0.224285,1.505066,-0.087041,-0.092538,1.505226,0.224300,-0.092539,-0.087041,-2.999998},{0.151965,1.379406,-0.091638,-0.102824,1.379563,0.151943,-0.102825,-0.091638,-2.999998},{0.151965,1.379406,-0.091638,-0.102824,1.379563,0.151943,-0.102825,-0.091638,-2.999998},{0.026644,0.985087,-0.100606,-0.121285,0.985210,0.026622,-0.121285,-0.100606,-2.999998},{0.026644,0.985087,-0.100606,-0.121285,0.985210,0.026622,-0.121285,-0.100606,-2.999998},{-0.206501,-0.297570,-0.106511,-0.138436,-0.297519,-0.206499,-0.138435,-0.106510,-2.999998},{-0.206501,-0.297570,-0.106511,-0.138436,-0.297519,-0.206499,-0.138435,-0.106510,-2.999998},{-0.853999,2.967755,-0.122007,0.793777,2.967979,-0.854176,0.793734,-0.122011,-2.999998},{-0.853999,2.967755,-0.122007,0.793777,2.967979,-0.854176,0.793734,-0.122011,-2.999998},{-0.426803,2.881358,-0.490669,2.999988,2.881179,-0.426824,2.999986,-0.490650,-2.999998},{-0.426803,2.881358,-0.490669,2.999988,2.881179,-0.426824,2.999986,-0.490650,-2.999998},{0.553930,2.999997,-0.119915,1.032105,2.999997,0.553895,1.031793,-0.119905,-2.999995},{0.553930,2.999997,-0.119915,1.032105,2.999997,0.553895,1.031793,-0.119905,-2.999995},{1.875843,2.999996,1.516979,2.999997,2.999996,1.875521,2.999996,1.516924,-2.694158},{1.875843,2.999996,1.516979,2.999997,2.999996,1.875521,2.999996,1.516924,-2.694158},{2.277567,2.999983,2.999960,2.999989,2.999983,2.277579,2.999989,2.999964,-2.140722},{-1.920164,-2.999164,2.999902,2.887310,-2.999144,-1.920167,2.887290,2.999900,-2.114981},{-1.920164,-2.999164,2.999902,2.887310,-2.999144,-1.920167,2.887290,2.999900,-2.114981},{-2.370053,-2.999505,2.999916,2.816547,-2.999507,-2.370033,2.816533,2.999916,-2.057854},{-2.370053,-2.999505,2.999916,2.816547,-2.999507,-2.370033,2.816533,2.999916,-2.057854},{-2.285281,-2.999501,2.999913,2.754219,-2.999507,-2.285259,2.754209,2.999913,-2.002037},{-2.285281,-2.999501,2.999913,2.754219,-2.999507,-2.285259,2.754209,2.999913,-2.002037},{-2.227712,-2.999500,2.999911,2.700303,-2.999508,-2.227689,2.700294,2.999911,-1.947711},{-2.227712,-2.999500,2.999911,2.700303,-2.999508,-2.227689,2.700294,2.999911,-1.947711},{-2.194116,-2.999497,2.999908,2.653887,-2.999505,-2.194092,2.653880,2.999909,-1.894778},{-2.194116,-2.999497,2.999908,2.653887,-2.999505,-2.194092,2.653880,2.999909,-1.894778},{-2.179345,-2.999487,2.999906,2.614128,-2.999496,-2.179321,2.614123,2.999906,-1.843144},{-2.179345,-2.999487,2.999906,2.614128,-2.999496,-2.179321,2.614123,2.999906,-1.843144},{-2.193938,-2.999484,2.999903,2.580269,-2.999494,-2.193909,2.580266,2.999904,-1.792706},{-2.193938,-2.999484,2.999903,2.580269,-2.999494,-2.193909,2.580266,2.999904,-1.792706},{-2.224287,-2.999510,2.999901,2.551596,-2.999518,-2.224274,2.551596,2.999901,-1.743374},{-2.224287,-2.999510,2.999901,2.551596,-2.999518,-2.224274,2.551596,2.999901,-1.743374},{-2.273310,-2.999515,2.999899,2.527473,-2.999539,-2.273233,2.527476,2.999899,-1.695061},{-2.273310,-2.999515,2.999899,2.527473,-2.999539,-2.273233,2.527476,2.999899,-1.695061},{-2.334673,-2.999573,2.999897,2.506551,-2.999445,-2.334868,2.506559,2.999897,-1.646456},{-2.334673,-2.999573,2.999897,2.506551,-2.999445,-2.334868,2.506559,2.999897,-1.646456},{0.438935,-2.998501,-0.558863,-1.064325,-2.999402,0.440086,-1.064342,-0.558869,-1.629126}};
float dfVec[TIMESTEPS][MOTORS] = {{-0.005957,0.033897,-0.016083,-0.016316,0.033914,-0.005953,-0.016317,-0.016083,-0.585647},{-0.016916,0.018780,-0.017733,-0.022093,0.018797,-0.016912,-0.022093,-0.017732,-0.624130},{-0.011314,0.080567,-0.020695,-0.028325,0.080591,-0.011311,-0.028325,-0.020694,-0.660807},{-0.021520,0.067738,-0.024332,-0.034154,0.067762,-0.021517,-0.034154,-0.024332,-0.696167},{0.004020,0.244114,-0.031632,-0.041501,0.244145,0.004023,-0.041501,-0.031632,-0.726757},{-0.001646,0.241988,-0.041918,-0.047884,0.242019,-0.001643,-0.047884,-0.041918,-0.751442},{-0.022337,0.214337,-0.053451,-0.056747,0.214368,-0.022341,-0.056747,-0.053451,-0.776419},{-0.029663,0.210589,-0.064873,-0.064332,0.210620,-0.029667,-0.064331,-0.064873,-0.801342},{-0.064171,0.125893,-0.078117,-0.076556,0.125917,-0.064175,-0.076555,-0.078116,-0.828110},{-0.076133,0.115745,-0.090848,-0.086883,0.115770,-0.076137,-0.086882,-0.090847,-0.855548},{-0.146517,-0.165041,-0.101790,-0.101982,-0.165031,-0.146517,-0.101980,-0.101789,-0.891097},{-0.207112,-0.249533,-0.106081,-0.114712,-0.249523,-0.207111,-0.114709,-0.106080,-0.938842},{-0.376757,0.337683,-0.133176,0.049197,0.337727,-0.376791,0.049192,-0.133176,-0.955726},{-0.342791,0.407088,-0.189165,0.010699,0.407135,-0.342821,0.010695,-0.189163,-0.938653},{-0.151673,0.552594,-0.293411,0.417723,0.552563,-0.151671,0.417728,-0.293403,-0.940707},{-0.016602,0.741083,-0.254588,0.473310,0.741049,-0.016606,0.473315,-0.254581,-0.965257},{0.271637,0.897095,-0.145404,0.160286,0.897096,0.271627,0.160225,-0.145404,-0.968756},{0.292217,0.918834,-0.151045,0.196340,0.918835,0.292207,0.196262,-0.151059,-0.955664},{0.529244,0.870425,0.296403,0.764988,0.870425,0.529178,0.764969,0.296374,-0.916015},{0.513100,0.818673,0.486703,0.959352,0.818672,0.513033,0.959352,0.486688,-0.994527},{0.582549,0.775510,0.844034,0.953156,0.775510,0.582547,0.953161,0.844037,-0.989828},{-0.237448,-0.397195,0.850056,0.941081,-0.397191,-0.237448,0.941077,0.850056,-0.984223},{-0.243927,-0.407017,0.852059,0.938502,-0.407013,-0.243928,0.938499,0.852059,-0.989603},{-0.338705,-0.416955,0.853937,0.921988,-0.416955,-0.338701,0.921986,0.853937,-0.983800},{-0.346231,-0.426907,0.855692,0.919177,-0.426907,-0.346228,0.919175,0.855692,-0.989119},{-0.337066,-0.436754,0.857326,0.904108,-0.436755,-0.337062,0.904106,0.857326,-0.983472},{-0.344305,-0.446502,0.858845,0.901126,-0.446503,-0.344301,0.901124,0.858845,-0.988646},{-0.340202,-0.456163,0.860260,0.887550,-0.456165,-0.340198,0.887548,0.860260,-0.983148},{-0.347234,-0.465742,0.861576,0.884440,-0.465744,-0.347230,0.884438,0.861576,-0.988185},{-0.347639,-0.475251,0.862803,0.872220,-0.475253,-0.347634,0.872219,0.862803,-0.982827},{-0.354533,-0.484694,0.863947,0.869020,-0.484696,-0.354528,0.869019,0.863947,-0.987736},{-0.358502,-0.494080,0.865014,0.858027,-0.494082,-0.358498,0.858026,0.865014,-0.982508},{-0.365311,-0.503414,0.866010,0.854769,-0.503416,-0.365307,0.854768,0.866010,-0.987297},{-0.374966,-0.512709,0.866941,0.844884,-0.512711,-0.374960,0.844883,0.866941,-0.982189},{-0.381769,-0.521969,0.867813,0.841595,-0.521971,-0.381763,0.841595,0.867813,-0.986869},{-0.394515,-0.531207,0.868630,0.832703,-0.531209,-0.394512,0.832703,0.868631,-0.981871},{-0.401355,-0.540417,0.869398,0.829405,-0.540418,-0.401352,0.829405,0.869398,-0.986451},{-0.417809,-0.549613,0.870120,0.821402,-0.549617,-0.417794,0.821403,0.870120,-0.981554},{-0.424738,-0.558797,0.870800,0.818116,-0.558801,-0.424723,0.818116,0.870800,-0.986041},{-0.443708,-0.567988,0.871442,0.810756,-0.567963,-0.443746,0.810757,0.871442,-0.980997},{-0.451295,-0.577366,0.872314,0.807641,-0.577341,-0.451333,0.807642,0.872314,-0.985628},{0.085686,-0.585351,0.176827,0.106552,-0.585527,0.085910,0.106548,0.176826,-0.985452}};


class TrajAccelerate : public ReorientableBehavior
{
public:
	Interpolator interp;

	TAMode mode = TA_SIT; //Current state within state-machine
	int logging = 0;

	uint32_t tLast; //int used to store system time at various events

	float extDes; //The desired leg extension
	float angDes; // The desired leg angle
	float t;
	uint32_t tOld;
	uint32_t tNew;
	uint32_t timeStep;
	uint32_t iter = 0;

	bool boolSlow = false;

	float posDes[MOTORS];
	float velDes[MOTORS];
	float uDes[MOTORS];
	float df[MOTORS];
	float dfDes[MOTORS];
	float posAct;
	float velAct;
	float kp = 0.4; // 0.80
	float kd = 0.01; // 0.02
	float kpt = 1;
	float kdt = 0.01; // 0.02
	float kpy = 0.1;	//0.1;
	float kdy = 0;		//0.05;
	float kiy = 0.02;
	float V = 15;
	float R = 0.23;
	float kt = 0.0954;
	float yawErrorInt = 0;

	float yawInit;
	float yawDes;
	float turnLeft;

	float finalTime = times[TIMESTEPS - 1];

	// Parser state (for serial comms with raspi)
	// Goes from 0 to 1 to 2
	int numAlignmentSeen = 0;
	uint16_t rxPtr = 0;

	// Receive buffer and alignment
	const static uint16_t RX_BUF_SIZE = 100;
	SerialCommandPacket packet;

	//sig is mapped from remote; here, 3 corresponds to pushing the left stick to the right
	// which in turn forces the state machine into FH_LEAP
	void signal(uint32_t sig)
	{
		// tLast = S->millis;
		if (logging == 0) power_int = 0;
		// logging = 1;
		if (mode == TA_STAND)
		{
			mode = TA_GO;			// Start behavior in STAND mode
			tLast = S->millis;
		}	
	}

	void begin()
	{
		mode = TA_STAND;			// Start behavior in STAND mode
		tLast = S->millis;		// Record the system time @ this transition
		yawInit = S->imu.euler.z;
		yawErrorInt = 0;
		power_int = 0;
		// ioctl(LOGGER_FILENO, 0);//stop
	}

	void update()
	{
		tOld = tNew;
		tNew = clockTimeUS;
		timeStep = tNew - tOld;
		// if (tNew!=clockTimeUS)
		// {
		// 	tOld = tNew;
		// 	tNew = clockTimeUS;
		// 	timeStep = tNew - tOld;
		// }


		// Serial comms code
		oldvoltage = voltage;
		oldcurrent = current;

		// Character to store the latest received character
		uint8_t latestRX;

		// Loop through while there are new bytes available
		while (read(SERIAL_AUX_FILENO, &latestRX, 1) > 0)
		{
			if (numAlignmentSeen == 0 && latestRX == ALIGNMENT_WORD[0])
			{
				numAlignmentSeen = 1;
			}
			else if (numAlignmentSeen == 1 && latestRX == ALIGNMENT_WORD[1])
			{
				numAlignmentSeen = 2;
				rxPtr = 0;
			}
			else if (numAlignmentSeen == 2)
			{
				// Add the next byte to our memory space in serial_packet
				uint8_t *pSerialPacket = (uint8_t *)&packet;
				pSerialPacket[rxPtr++] = latestRX; // post-increment rxPtr

				// Check if we have read a whole packet
				if (rxPtr == sizeof(SerialCommandPacket))
				{
					// Copy voltage and current readings
					voltage = packet.voltage;
					current = packet.current;

					// Reset
					numAlignmentSeen = rxPtr = 0;
				}
			}
		}

		if (isReorienting())
			return;

		for (int j = 0; j < MOTORS; j++)
		{
			posDes[j] = 0;
			velDes[j] = 0;
			uDes[j] = 0;
			df[j] = 0;
		}

		if (mode == TA_SIT)
		{


			C->mode = RobotCommand_Mode_JOINT;
			for (int i = 0; i < P->joints_count; i++)
			{
				posDes[i] = 0.5;
				// Splay angle for the front/rear legs (outward splay due to fore-displacement of front legs
				// and aft-displacement of rear legs)
				// The pitch angle (S->imu.euler.y) is subtracted since we want to the set the *absolute* leg angle
				// and limb[i].setPosition(ANGLE, *) will set the angle of the leg *relative* to the robot body
				if(i==1 || i==3 || i==4 || i==6)
				{
					posDes[i] = posDes[i] - S->imu.euler.y;
				} else if(i==0 || i==2 || i==5 || i==7)
				{
					posDes[i] = posDes[i] + S->imu.euler.y;
				} else if (i == TAIL_MOT){
					posDes[i] = 0.0;
				}

				joint[i].setGain(0.8, .003);
				joint[i].setPosition(posDes[i]);
			}
		}
		else if (mode == TA_STAND)
		{


			C->mode = RobotCommand_Mode_JOINT;
			for (int i = 0; i < P->joints_count; i++)
			{
				posDes[i] = 0.5;
				// Splay angle for the front/rear legs (outward splay due to fore-displacement of front legs
				// and aft-displacement of rear legs)
				// The pitch angle (S->imu.euler.y) is subtracted since we want to the set the *absolute* leg angle
				// and limb[i].setPosition(ANGLE, *) will set the angle of the leg *relative* to the robot body
				if(i==1 || i==3 || i==4 || i==6 || i==TAIL_MOT)
				{
					posDes[i] = pos[0][i] - S->imu.euler.y;
				} else if(i==0 || i==2 || i==5 || i==7)
				{
					posDes[i] = pos[0][i] + S->imu.euler.y;
				}

				joint[i].setGain(0.8, .003);
				joint[i].setPosition(posDes[i]);
			}
		}
		else if (mode == TA_GO)
		{
			// if (voltage<10)
			// {
			// 	voltage = oldvoltage;
			// 	current = oldcurrent;
			// }
			// power_int = power_int + 0.000001*timeStep*(voltage + oldvoltage)*0.5*(current + oldcurrent)*0.5;

			C->mode = RobotCommand_Mode_JOINT;

			// t = 0.00001 * (100 * (S->millis - tLast) % (int)(100000 * finalTime));
			t = 0.001*(S->millis - tLast);

			interp.getMultipleCubicSplineInterp(pos, vel, times, t, posDes);
			interp.getMultipleLinearInterp(vel, times, t, velDes);
			// interp.getMultipleZOH(u, times, t, uDes);
			interp.getMultipleZOH(dfVec, times, t, dfDes);

			for (int i = 0; i < MOTORS; ++i)
			{
				posAct = joint[i].getPosition();
				velAct = joint[i].getVelocity();

				// df[i] = 1 / (0.95 * V) * (uDes[i] * R / kt + kt * joint[i].getVelocity());
				joint[i].setOpenLoop(dfDes[i] + kp*(posDes[i] - posAct) + kd*(velDes[i] - velAct));

				if (i == TAIL_MOT)
				{
					// df[i] = 1/(0.95*V)*(uDes[i]*Ra/kt + kt*45.39*joint[i].getVelocity());													// Feedforward torques
					joint[i].setOpenLoop(dfDes[i] + kpt*(posDes[i] - posAct) + kdt*(velDes[i] - velAct));											// Feedforward duty factors
					// joint[i].setOpenLoop(df[i] + kpt*(posDes[i] - posAct) + kdt*(velDes[i] - velAct) + kppc*(S->imu.euler.y - pitchDes) + kdpc*(S->imu.angular_velocity.y - DpitchDes));	// Feedforward duty factors with pitch control

				} else {
					// dfDes[i] = 1/(0.95*V)*(uDes[i]*Ra/kt + kt*joint[i].getVelocity());
					joint[i].setOpenLoop(dfDes[i] + kp*(posDes[i] - posAct) + kd*(velDes[i] - velAct));
				}

				if (t>=times[TIMESTEPS-1])
				{
					mode = TA_SIT;
					tLast = S->millis;
					t = 0;
				}
			}
		}
	}

	bool running()
	{
		return false;
	}
	void end()
	{
		mode = TA_SIT;
		logging = 0;
	}
};

TrajAccelerate trajAccelerate;

void debug()
{
	// printf("loop: ");
	// printf("%d, ", trajAccelerate.mode);
	// printf("pos command: %4.3f, %4.3f, %4.3f, %4.3f, %4.3f, %4.3f, %4.3f, %4.3f. ",
	// 	trajAccelerate.uDes[0],trajAccelerate.uDes[1],trajAccelerate.uDes[2],trajAccelerate.uDes[3],trajAccelerate.uDes[4],trajAccelerate.uDes[5],trajAccelerate.uDes[6],trajAccelerate.uDes[7]);
	// printf("Time: %4.3fs. ", trajAccelerate.t);
	// printf("%4.3f, %4.3f, %4.3f ", trajAccelerate.yawInit, trajAccelerate.turnLeft, trajAccelerate.kiy * trajAccelerate.yawErrorInt);
	// printf("test1, test2: %4.3f, %4.3f.", trajAccelerate.test1,trajAccelerate.test2);
	// printf("Voltage: %6.3f, Current: %6.3f", voltage,  current);

	// for (int i = 0; i < P->joints_count; i++)
	// {
	// 	printf("Motor %d, Pos: %4.3f, ", i, joint[i].getRawPosition());
	// }

	printf("Motor %d, RawPos: %4.3f, ", TAIL_MOT, joint[TAIL_MOT].getRawPosition());
	printf("Motor %d, Pos: %4.3f, ", TAIL_MOT, joint[TAIL_MOT].getPosition());
	printf("Motor %d, cmd: %4.3f, ", TAIL_MOT, joint[TAIL_MOT].getOpenLoop());

	// printf("Power Int: %6.2f, Voltage: %5.2f, Current: %5.2f", power_int, voltage, current);
	printf("\n");

	// myData_buf[0] = trajAccelerate.logging;
	// myData_buf[1] = voltage;
	// myData_buf[2] = current;
	// myData_buf[3] = power_int;
	// write(LOGGER_FILENO, myData, 32);
}

int main(int argc, char *argv[])
{
// #if defined(ROBOT_MINITAUR)
// 	init(RobotParams_Type_MINITAUR, argc, argv);
// 	for (int i = 0; i < P->joints_count; ++i)
// 		P->joints[i].zero = motZeros[i]; //Add motor zeros from array at beginning of file
// #elif defined(ROBOT_MINITAUR_E)
// 	init(RobotParams_Type_MINITAUR_E, argc, argv);
// 	JoyType joyType = JoyType_FRSKY_XSR;
// 	ioctl(JOYSTICK_FILENO, IOCTL_CMD_JOYSTICK_SET_TYPE, &joyType);
// #else
// #error "Define robot type in preprocessor"
// #endif

	#if defined(ROBOT_MINITAUR)
		init(RobotParams_Type_MINITAUR, argc, argv);

			// Set the joint type; see JointParams
		P->joints[TAIL_MOT].type = JointParams_Type_GRBL;

		// Set the *physical* address (e.g. etherCAT ID, PWM port, dynamixel ID, etc.)
		P->joints[TAIL_MOT].address = TAIL_MOT+1;

		// If there is a gearbox the joint electronics doesn't know about, this could be > 1.
		// Do not set to 0.
		P->joints[TAIL_MOT].gearRatio = 4;

		// Configure joints
		P->joints_count = S->joints_count = C->joints_count = MOTORS;
		for (int i = 0; i < P->joints_count; i++)
		{
			// Set zeros and directions
			P->joints[i].zero = motZeros[i]; //Add motor zeros from array at beginning of file
		}
		P->joints[TAIL_MOT].direction = 1;
			
	#elif defined(ROBOT_MINITAUR_E)
		init(RobotParams_Type_MINITAUR_E, argc, argv);
		JoyType joyType = JoyType_FRSKY_XSR;
		ioctl(JOYSTICK_FILENO, IOCTL_CMD_JOYSTICK_SET_TYPE, &joyType);
	#else
	#error "Define robot type in preprocessor"
	#endif


	// Uncomment to clear Accelerate and Walk behaviors
	// behaviors.clear();
	
	behaviors.push_back(&trajAccelerate);

	setDebugRate(50);
	safetyShutoffEnable(false);

	SerialPortConfig cfg;
	cfg.baud = 115200;
	cfg.mode = SERIAL_8N1;
	ioctl(STDOUT_FILENO, IOCTL_CMD_SERIAL_PORT_CFG, &cfg);


	// Run
	return begin();
}