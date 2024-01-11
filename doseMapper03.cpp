/*
* ����ӳ�� color-map
*   ������Ҫ������
*           1.��ȡ�ļ�dose1.txt���������ά����ddata��
*           2.������ɫӳ���lookupTable
*           3.�趨��ֵ��������ֵ���ˣ����ڱ���
*           4.��˹ģ������
*           5.��ֵ��
*           6.ɫ������ scalerBar
* ����ʱ�䣺2023/12/11
* writeBy��Longquan Jin
*/
#include <vtkDICOMImageReader.h>
#include <vtkSmartPointer.h>
#include <vtkImageData.h>
#include <vtkImageActor.h>
#include <vtkRenderer.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkLookupTable.h>
#include <vtkScalarBarActor.h>
#include <vtkTextProperty.h>
#include <vtkImageMapToColors.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkDoubleArray.h>
#include <vtkPointData.h>
#include <vtkImageMapper3D.h>
#include <vtkContourFilter.h>  // ��ֵ��
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkImageReslice.h>
#include <vtkTransform.h>

#include <iostream>
#include <fstream>
#include <string>
#include <limits>

int main(int argc, char* argv[])
{

    vtkSmartPointer<vtkDICOMImageReader> reader = vtkSmartPointer<vtkDICOMImageReader>::New();
    reader->SetFileName("D:\\DICOM\\vhf\\vhf.1626.dcm");
    reader->Update();

    // ��ʱ����ת90�ȵı任����
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->PostMultiply(); // ���ñ任����ĺ��ģʽ�������ж��ֱ任������ 
    transform->RotateZ(90.0);  // ��Z����ʱ����ת90��
    transform->Translate(512, 0, 0);

    // ΪDICOMͼ�񴴽� actor
    vtkSmartPointer<vtkImageActor> dcmActor = vtkSmartPointer<vtkImageActor>::New();
    dcmActor->SetUserTransform(transform);
    dcmActor->GetMapper()->SetInputConnection(reader->GetOutputPort());

    /*
    * ��������ddata����dose1.txt�е����ݱ��浽��ά����ddata��
    */
    const int dim = 512;
    double** ddata = new double* [dim];
    for (int i = 0; i < dim; i++) {
        ddata[i] = new double[dim];
    }

    std::ifstream file("./doseNew.txt");
    int row = 0;
    int col = 0;

    if (file.is_open())
    {
        std::string line;
        while (getline(file, line))
        {
            double doseValue = std::stod(line);

            ddata[row][col] = doseValue;

            col++;
            if (col == dim)
            {
                col = 0;
                row++;
            }
        }
        file.close();
    }
    else
    {
        std::cerr << "Unable to open file" << std::endl;
        return EXIT_FAILURE;
    }

    /*
    * Ѱ�������е������Сֵ
    */
    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::min();

    for (int i = 0; i < dim; i++) {
        for (int j = 0; j < dim; j++) {
            minValue = std::min(minValue, ddata[i][j]);
            maxValue = std::max(maxValue, ddata[i][j]);
        }
    }

    /*
    * 1.�趨��ֵ threshold
    * 2.������ɫӳ��� lookupTable
    */
    const double threshold = 0.00000034;
    vtkSmartPointer<vtkLookupTable> lookupTable = vtkSmartPointer<vtkLookupTable>::New();
    lookupTable->SetRange(threshold, maxValue);        // ����ӳ���Χ  * 0.48
    lookupTable->SetNumberOfTableValues(1024);         // ������ɫ����Ϊ1024
    lookupTable->SetHueRange(0.6667, 0.0);             // ��ɫ��Χ������ɫ����ɫ
    lookupTable->SetSaturationRange(1.0, 1.0);         // ���Ͷȷ�Χ�����ֲ���
    lookupTable->SetValueRange(1.0, 1.0);              // ���ȷ�Χ�����ֲ���
    lookupTable->Build();

    /*
    * �޸�͸����
    */
    for (int i = 0; i < lookupTable->GetNumberOfTableValues(); ++i) {
        double rgba[4];
        lookupTable->GetTableValue(i, rgba);
        rgba[3] = rgba[3] * 0.3;  // ����͸���ȣ�����Ϊԭ����50%
        lookupTable->SetTableValue(i, rgba);
    }


    /*
    * ���˲���
    * ��ֵС����ֵ����ɫӳ��Ϊɫ�귶Χ��Сֵ
    */
    vtkSmartPointer<vtkImageData> coloredDoseData = vtkSmartPointer<vtkImageData>::New();
    coloredDoseData->SetDimensions(dim, dim, 1);
    coloredDoseData->AllocateScalars(VTK_DOUBLE, 1);

    vtkSmartPointer<vtkDoubleArray> doseArray = vtkSmartPointer<vtkDoubleArray>::New();
    doseArray->SetNumberOfComponents(1);
    doseArray->SetName("DoseArray");

    for (int y = 0; y < dim; y++) {
        for (int x = 0; x < dim; x++) {
            if (ddata[x][y] >= threshold) {
                doseArray->InsertNextValue(ddata[x][y]);
            }
            else {
                doseArray->InsertNextValue(0.0);
            }
        }
    }

    coloredDoseData->GetPointData()->SetScalars(doseArray);

    /*
    * ��˹�˲�ƽ��
    * 1.��׼�standard deviation��
    */
    vtkSmartPointer<vtkImageGaussianSmooth> gaussianSmooth = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    gaussianSmooth->SetInputData(coloredDoseData);
    gaussianSmooth->SetStandardDeviation(6.0);        // ���ò�ͬ��׼��Ի�ȡ���ģ��Ч��
    gaussianSmooth->SetRadiusFactors(1.5, 1.2, 0.1);  // ���ø�˹���߰뾶����
    gaussianSmooth->Update();

    /*
    * ������ֵ�߶��� contourFilter
    */
    vtkSmartPointer<vtkContourFilter> contourFilter = vtkSmartPointer<vtkContourFilter>::New();
    contourFilter->SetInputConnection(gaussianSmooth->GetOutputPort());
    contourFilter->GenerateValues(5, threshold, maxValue); // ��һ�����������ɵ�ֵ�ߵ�����
    contourFilter->Update();

    vtkSmartPointer<vtkPolyDataMapper> contourMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    contourMapper->SetInputConnection(contourFilter->GetOutputPort());
    contourMapper->ScalarVisibilityOn();
    contourMapper->SetScalarRange(threshold, maxValue);

    vtkSmartPointer<vtkActor> contourActor = vtkSmartPointer<vtkActor>::New();
    contourActor->SetMapper(contourMapper);
    contourActor->GetProperty()->SetLineWidth(2);

    
    /*
    * ʹ����ɫӳ�����ͼ���еļ���ֵӳ��Ϊ��ɫ
    */
    vtkSmartPointer<vtkImageMapToColors> mapColors = vtkSmartPointer<vtkImageMapToColors>::New();
    mapColors->SetLookupTable(lookupTable);
    mapColors->SetInputConnection(gaussianSmooth->GetOutputPort());

    /*
    * Ϊ����ӳ�����ݴ���һ��actor
    */
    vtkSmartPointer<vtkImageActor> doseActor = vtkSmartPointer<vtkImageActor>::New();
    doseActor->GetMapper()->SetInputConnection(mapColors->GetOutputPort());

    /*
    * Renderer& Render Window
    * 
    * ˳��ʽ��
    *       1.��Ⱦ�� renderer��renderer��
    *       2.��Ⱦ���� render window(renWin)
    *       3.����Ⱦ����ӵ���Ⱦ������ȥ -->> renWin->AddRenderer(renderer);
    *       4.Ϊ��Ⱦ�����actor������ -->> renderer->AddActor(xxxActor);
    *       5.����Ⱦ���ڽ�������ӵ���Ⱦ������ȥ -->> renWin->SetInteractor(interactor);
    */ 
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
    renWin->AddRenderer(renderer);
    renWin->SetSize(800, 800);

    // Add Actor to the renderer
    renderer->AddActor(dcmActor);      // DICOM actor
    renderer->AddActor(contourActor);  // ��ֵ�� actor
    renderer->AddActor(doseActor);     // ������ͼ actor

    /*
    * ����ɫ�� scalerBar
    */
    vtkSmartPointer<vtkScalarBarActor> scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar->SetLookupTable(lookupTable);
    scalarBar->SetNumberOfLabels(5);
    scalarBar->SetTitle("Neutron flux(n/cm^2/s)");
    renderer->AddActor(scalarBar);

    /*
    * ��Ⱦ���ڽ����� Render Window Interactor ��interactor��
    */
    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    renWin->SetInteractor(interactor);

    // Start the visualization
    renderer->ResetCamera();
    renWin->Render();
    interactor->Start();

    /*
    * �ͷŶ�ά����ddata�ڴ�
    */
    for (int i = 0; i < dim; i++) {
        delete[] ddata[i];
    }
    delete[] ddata;

    return EXIT_SUCCESS;
}
