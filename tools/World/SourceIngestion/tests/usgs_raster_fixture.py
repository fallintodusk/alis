"""Captured native USGS header fields and FGDC spatial/distribution evidence; no raw decode."""

RASTERS = [{'file_name': 'USGS_13_n41w074_20260917.tif',
  'url': 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/13/TIFF/historical/n41w074/USGS_13_n41w074_20260917.tif',
  'header': {'coordinateSystem': {'wkt': 'GEOGCRS["NAD83",\n'
                                         '    DATUM["North American Datum 1983",\n'
                                         '        ELLIPSOID["GRS 1980",6378137,298.257222101004,\n'
                                         '            LENGTHUNIT["metre",1]]],\n'
                                         '    PRIMEM["Greenwich",0,\n'
                                         '        ANGLEUNIT["degree",0.0174532925199433]],\n'
                                         '    CS[ellipsoidal,2],\n'
                                         '        AXIS["geodetic latitude (Lat)",north,\n'
                                         '            ORDER[1],\n'
                                         '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                         '        AXIS["geodetic longitude (Lon)",east,\n'
                                         '            ORDER[2],\n'
                                         '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                         '    ID["EPSG",4269]]',
                                  'dataAxisToSRSAxisMapping': [2, 1]},
             'stac': {'proj:shape': [10812, 10812],
                      'proj:wkt2': 'GEOGCRS["NAD83",\n'
                                   '    DATUM["North American Datum 1983",\n'
                                   '        ELLIPSOID["GRS 1980",6378137,298.257222101004,\n'
                                   '            LENGTHUNIT["metre",1]]],\n'
                                   '    PRIMEM["Greenwich",0,\n'
                                   '        ANGLEUNIT["degree",0.0174532925199433]],\n'
                                   '    CS[ellipsoidal,2],\n'
                                   '        AXIS["geodetic latitude (Lat)",north,\n'
                                   '            ORDER[1],\n'
                                   '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                   '        AXIS["geodetic longitude (Lon)",east,\n'
                                   '            ORDER[2],\n'
                                   '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                   '    ID["EPSG",4269]]',
                      'proj:epsg': 4269,
                      'proj:projjson': {'$schema': 'https://proj.org/schemas/v0.7/projjson.schema.json',
                                        'type': 'GeographicCRS',
                                        'name': 'NAD83',
                                        'datum': {'type': 'GeodeticReferenceFrame',
                                                  'name': 'North American Datum 1983',
                                                  'ellipsoid': {'name': 'GRS 1980',
                                                                'semi_major_axis': 6378137,
                                                                'inverse_flattening': 298.257222101004}},
                                        'coordinate_system': {'subtype': 'ellipsoidal',
                                                              'axis': [{'name': 'Geodetic latitude',
                                                                        'abbreviation': 'Lat',
                                                                        'direction': 'north',
                                                                        'unit': 'degree'},
                                                                       {'name': 'Geodetic longitude',
                                                                        'abbreviation': 'Lon',
                                                                        'direction': 'east',
                                                                        'unit': 'degree'}]},
                                        'id': {'authority': 'EPSG', 'code': 4269}},
                      'proj:transform': [-74.00055555659299,
                                         9.25925927754e-05,
                                         0.0,
                                         41.000555556194854,
                                         0.0,
                                         -9.25925926922e-05],
                      'raster:bands': [{'data_type': 'float32',
                                        'stats': {'minimum': -9.182,
                                                  'maximum': 169.88,
                                                  'mean': 15.515,
                                                  'stddev': 22.438},
                                        'nodata': None}],
                      'eo:bands': [{'name': 'b1', 'description': 'Gray'}]},
             'size': [10812, 10812],
             'geoTransform': [-74.00055555659299,
                              9.25925927754e-05,
                              0.0,
                              41.000555556194854,
                              0.0,
                              -9.25925926922e-05],
             'metadata': {'': {'BandDefinitionKeyword': '*', 'DataType': '*', 'AREA_OR_POINT': 'Area'},
                          'IMAGE_STRUCTURE': {'LAYOUT': 'COG',
                                              'COMPRESSION': 'LZW',
                                              'INTERLEAVE': 'BAND',
                                              'PREDICTOR': '3'}},
             'bands': [{'band': 1,
                        'block': [512, 512],
                        'type': 'Float32',
                        'colorInterpretation': 'Gray',
                        'min': -9.182,
                        'max': 169.88,
                        'minimum': -9.182,
                        'maximum': 169.88,
                        'mean': 15.515,
                        'stdDev': 22.438,
                        'noDataValue': -999999.0,
                        'overviews': [{'size': [5406, 5406]},
                                      {'size': [2703, 2703]},
                                      {'size': [1351, 1351]},
                                      {'size': [675, 675]},
                                      {'size': [337, 337]}],
                        'metadata': {'': {'LAYER_TYPE': '*',
                                          'RepresentationType': '*',
                                          'STATISTICS_MAXIMUM': '169.87953186035',
                                          'STATISTICS_MEAN': '15.514818319371',
                                          'STATISTICS_MINIMUM': '-9.1815090179443',
                                          'STATISTICS_STDDEV': '22.438411476019',
                                          'STATISTICS_VALID_PERCENT': '49.35'}}}]},
  'fgdc': '<metadata><spref>\n'
          '    <horizsys>\n'
          '      <geograph>\n'
          '        <latres>-9.259259269220167e-05</latres>\n'
          '        <longres>9.25925927753796e-05</longres>\n'
          '        <geogunit>Decimal degrees</geogunit>\n'
          '      </geograph>\n'
          '      <geodetic>\n'
          '        <horizdn>North American Datum of 1983</horizdn>\n'
          '        <ellips>Geodetic Reference System 80</ellips>\n'
          '        <semiaxis>6378137.000000</semiaxis>\n'
          '        <denflat>298.2572221</denflat>\n'
          '      </geodetic>\n'
          '    </horizsys>\n'
          '    <vertdef>\n'
          '      <altsys>\n'
          '        <altdatum>North American Vertical Datum of 1988</altdatum>\n'
          '        <altres>1.000000</altres>\n'
          '        <altunits>meters</altunits>\n'
          '        <altenc>Implicit coordinate</altenc>\n'
          '      </altsys>\n'
          '    </vertdef>\n'
          '  </spref>\n'
          '  <distinfo><stdorder>\n'
          '      <digform>\n'
          '        <digtinfo>\n'
          '          <formname>TIFF</formname>\n'
          '          <transize>158.09657096862793</transize>\n'
          '        </digtinfo>\n'
          '        <digtopt>\n'
          '          <onlinopt>\n'
          '            <computer>\n'
          '              <networka>\n'
          '                '
          '<networkr>https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/13/TIFF/historical/n41w074/USGS_13_n41w074_20260917.tif</networkr>\n'
          '              </networka>\n'
          '            </computer>\n'
          '          </onlinopt>\n'
          '        </digtopt>\n'
          '      </digform>\n'
          '      <fees>NONE</fees>\n'
          '    </stdorder>\n'
          '  </distinfo></metadata>'},
 {'file_name': 'USGS_13_n41w075_20261028.tif',
  'url': 'https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/13/TIFF/historical/n41w075/USGS_13_n41w075_20261028.tif',
  'header': {'coordinateSystem': {'wkt': 'GEOGCRS["NAD83",\n'
                                         '    DATUM["North American Datum 1983",\n'
                                         '        ELLIPSOID["GRS 1980",6378137,298.257222101004,\n'
                                         '            LENGTHUNIT["metre",1]]],\n'
                                         '    PRIMEM["Greenwich",0,\n'
                                         '        ANGLEUNIT["degree",0.0174532925199433]],\n'
                                         '    CS[ellipsoidal,2],\n'
                                         '        AXIS["geodetic latitude (Lat)",north,\n'
                                         '            ORDER[1],\n'
                                         '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                         '        AXIS["geodetic longitude (Lon)",east,\n'
                                         '            ORDER[2],\n'
                                         '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                         '    ID["EPSG",4269]]',
                                  'dataAxisToSRSAxisMapping': [2, 1]},
             'stac': {'proj:shape': [10812, 10812],
                      'proj:wkt2': 'GEOGCRS["NAD83",\n'
                                   '    DATUM["North American Datum 1983",\n'
                                   '        ELLIPSOID["GRS 1980",6378137,298.257222101004,\n'
                                   '            LENGTHUNIT["metre",1]]],\n'
                                   '    PRIMEM["Greenwich",0,\n'
                                   '        ANGLEUNIT["degree",0.0174532925199433]],\n'
                                   '    CS[ellipsoidal,2],\n'
                                   '        AXIS["geodetic latitude (Lat)",north,\n'
                                   '            ORDER[1],\n'
                                   '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                   '        AXIS["geodetic longitude (Lon)",east,\n'
                                   '            ORDER[2],\n'
                                   '            ANGLEUNIT["degree",0.0174532925199433]],\n'
                                   '    ID["EPSG",4269]]',
                      'proj:epsg': 4269,
                      'proj:projjson': {'$schema': 'https://proj.org/schemas/v0.7/projjson.schema.json',
                                        'type': 'GeographicCRS',
                                        'name': 'NAD83',
                                        'datum': {'type': 'GeodeticReferenceFrame',
                                                  'name': 'North American Datum 1983',
                                                  'ellipsoid': {'name': 'GRS 1980',
                                                                'semi_major_axis': 6378137,
                                                                'inverse_flattening': 298.257222101004}},
                                        'coordinate_system': {'subtype': 'ellipsoidal',
                                                              'axis': [{'name': 'Geodetic latitude',
                                                                        'abbreviation': 'Lat',
                                                                        'direction': 'north',
                                                                        'unit': 'degree'},
                                                                       {'name': 'Geodetic longitude',
                                                                        'abbreviation': 'Lon',
                                                                        'direction': 'east',
                                                                        'unit': 'degree'}]},
                                        'id': {'authority': 'EPSG', 'code': 4269}},
                      'proj:transform': [-75.00055555639312,
                                         9.25925926922e-05,
                                         0.0,
                                         41.000555556194854,
                                         0.0,
                                         -9.25925926922e-05],
                      'raster:bands': [{'data_type': 'float32',
                                        'stats': {'minimum': -37.371,
                                                  'maximum': 399.029,
                                                  'mean': 71.546,
                                                  'stddev': 79.208},
                                        'nodata': None}],
                      'eo:bands': [{'name': 'b1', 'description': 'Gray'}]},
             'size': [10812, 10812],
             'geoTransform': [-75.00055555639312,
                              9.25925926922e-05,
                              0.0,
                              41.000555556194854,
                              0.0,
                              -9.25925926922e-05],
             'metadata': {'': {'BandDefinitionKeyword': '*', 'DataType': '*', 'AREA_OR_POINT': 'Area'},
                          'IMAGE_STRUCTURE': {'LAYOUT': 'COG',
                                              'COMPRESSION': 'LZW',
                                              'INTERLEAVE': 'BAND',
                                              'PREDICTOR': '3'}},
             'bands': [{'band': 1,
                        'block': [512, 512],
                        'type': 'Float32',
                        'colorInterpretation': 'Gray',
                        'min': -37.371,
                        'max': 399.029,
                        'minimum': -37.371,
                        'maximum': 399.029,
                        'mean': 71.546,
                        'stdDev': 79.208,
                        'noDataValue': -999999.0,
                        'overviews': [{'size': [5406, 5406]},
                                      {'size': [2703, 2703]},
                                      {'size': [1351, 1351]},
                                      {'size': [675, 675]},
                                      {'size': [337, 337]}],
                        'metadata': {'': {'DESCRIPTION': '*',
                                          'LAYER_TYPE': '*',
                                          'RepresentationType': '*',
                                          'STATISTICS_MAXIMUM': '399.02890014648',
                                          'STATISTICS_MEAN': '71.546249640492',
                                          'STATISTICS_MINIMUM': '-37.371223449707',
                                          'STATISTICS_STDDEV': '79.207626929985',
                                          'STATISTICS_VALID_PERCENT': '100'}}}]},
  'fgdc': '<metadata><spref>\n'
          '    <horizsys>\n'
          '      <geograph>\n'
          '        <latres>-9.259259269220167e-05</latres>\n'
          '        <longres>9.259259269220167e-05</longres>\n'
          '        <geogunit>Decimal degrees</geogunit>\n'
          '      </geograph>\n'
          '      <geodetic>\n'
          '        <horizdn>North American Datum of 1983</horizdn>\n'
          '        <ellips>Geodetic Reference System 80</ellips>\n'
          '        <semiaxis>6378137.000000</semiaxis>\n'
          '        <denflat>298.2572221</denflat>\n'
          '      </geodetic>\n'
          '    </horizsys>\n'
          '    <vertdef>\n'
          '      <altsys>\n'
          '        <altdatum>North American Vertical Datum of 1988</altdatum>\n'
          '        <altres>1.000000</altres>\n'
          '        <altunits>meters</altunits>\n'
          '        <altenc>Implicit coordinate</altenc>\n'
          '      </altsys>\n'
          '    </vertdef>\n'
          '  </spref>\n'
          '  <distinfo><stdorder>\n'
          '      <digform>\n'
          '        <digtinfo>\n'
          '          <formname>TIFF</formname>\n'
          '          <transize>473.1411304473877</transize>\n'
          '        </digtinfo>\n'
          '        <digtopt>\n'
          '          <onlinopt>\n'
          '            <computer>\n'
          '              <networka>\n'
          '                '
          '<networkr>https://prd-tnm.s3.amazonaws.com/StagedProducts/Elevation/13/TIFF/historical/n41w075/USGS_13_n41w075_20261028.tif</networkr>\n'
          '              </networka>\n'
          '            </computer>\n'
          '          </onlinopt>\n'
          '        </digtopt>\n'
          '      </digform>\n'
          '      <fees>NONE</fees>\n'
          '    </stdorder>\n'
          '  </distinfo></metadata>'}]
