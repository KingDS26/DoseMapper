/*
* 剂量映射 color-map
*   内容主要包括：
*           1.读取文件dose1.txt，并存入二维数组ddata中
*           2.创建颜色映射表lookupTable
*           3.设定阈值，低于阈值过滤，高于保留
*           4.高斯模糊处理
*           5.等值线
*           6.色标文字 scalerBar
* 创建时间：2023/12/11
* writeBy：Longquan Jin
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
#include <vtkContourFilter.h>  // 等值线
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

    // 逆时针旋转90度的变换矩阵
    vtkSmartPointer<vtkTransform> transform = vtkSmartPointer<vtkTransform>::New();
    transform->PostMultiply(); // 设置变换对象的后乘模式（适用有多种变换操作） 
    transform->RotateZ(90.0);  // 沿Z轴逆时针旋转90度
    transform->Translate(512, 0, 0);

    // 为DICOM图像创建 actor
    vtkSmartPointer<vtkImageActor> dcmActor = vtkSmartPointer<vtkImageActor>::New();
    dcmActor->SetUserTransform(transform);
    dcmActor->GetMapper()->SetInputConnection(reader->GetOutputPort());

    /*
    * 创建数组ddata，将dose1.txt中的数据保存到二维数组ddata中
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
    * 寻找数组中的最大最小值
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
    * 1.设定阈值 threshold
    * 2.创建颜色映射表 lookupTable
    */
    const double threshold = 0.00000034;
    vtkSmartPointer<vtkLookupTable> lookupTable = vtkSmartPointer<vtkLookupTable>::New();
    lookupTable->SetRange(threshold, maxValue);        // 设置映射表范围  * 0.48
    lookupTable->SetNumberOfTableValues(1024);         // 设置颜色数量为1024
    lookupTable->SetHueRange(0.6667, 0.0);             // 颜色范围：从蓝色到红色
    lookupTable->SetSaturationRange(1.0, 1.0);         // 饱和度范围：保持不变
    lookupTable->SetValueRange(1.0, 1.0);              // 明度范围：保持不变
    lookupTable->Build();

    /*
    * 修改透明度
    */
    for (int i = 0; i < lookupTable->GetNumberOfTableValues(); ++i) {
        double rgba[4];
        lookupTable->GetTableValue(i, rgba);
        rgba[3] = rgba[3] * 0.3;  // 减少透明度，设置为原来的50%
        lookupTable->SetTableValue(i, rgba);
    }


    /*
    * 过滤操作
    * 将值小于阈值的颜色映射为色标范围最小值
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
    * 高斯滤波平滑
    * 1.标准差（standard deviation）
    */
    vtkSmartPointer<vtkImageGaussianSmooth> gaussianSmooth = vtkSmartPointer<vtkImageGaussianSmooth>::New();
    gaussianSmooth->SetInputData(coloredDoseData);
    gaussianSmooth->SetStandardDeviation(6.0);        // 设置不同标准差，以获取最佳模糊效果
    gaussianSmooth->SetRadiusFactors(1.5, 1.2, 0.1);  // 设置高斯曲线半径因子
    gaussianSmooth->Update();

    /*
    * 创建等值线对象 contourFilter
    */
    vtkSmartPointer<vtkContourFilter> contourFilter = vtkSmartPointer<vtkContourFilter>::New();
    contourFilter->SetInputConnection(gaussianSmooth->GetOutputPort());
    contourFilter->GenerateValues(5, threshold, maxValue); // 第一个参数是生成等值线的数量
    contourFilter->Update();

    vtkSmartPointer<vtkPolyDataMapper> contourMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    contourMapper->SetInputConnection(contourFilter->GetOutputPort());
    contourMapper->ScalarVisibilityOn();
    contourMapper->SetScalarRange(threshold, maxValue);

    vtkSmartPointer<vtkActor> contourActor = vtkSmartPointer<vtkActor>::New();
    contourActor->SetMapper(contourMapper);
    contourActor->GetProperty()->SetLineWidth(2);

    
    /*
    * 使用颜色映射表，将图像中的剂量值映射为颜色
    */
    vtkSmartPointer<vtkImageMapToColors> mapColors = vtkSmartPointer<vtkImageMapToColors>::New();
    mapColors->SetLookupTable(lookupTable);
    mapColors->SetInputConnection(gaussianSmooth->GetOutputPort());

    /*
    * 为剂量映射数据创建一个actor
    */
    vtkSmartPointer<vtkImageActor> doseActor = vtkSmartPointer<vtkImageActor>::New();
    doseActor->GetMapper()->SetInputConnection(mapColors->GetOutputPort());

    /*
    * Renderer& Render Window
    * 
    * 顺序方式：
    *       1.渲染器 renderer（renderer）
    *       2.渲染窗口 render window(renWin)
    *       3.将渲染器添加到渲染窗口中去 -->> renWin->AddRenderer(renderer);
    *       4.为渲染器添加actor或属性 -->> renderer->AddActor(xxxActor);
    *       5.将渲染窗口交互器添加到渲染窗口中去 -->> renWin->SetInteractor(interactor);
    */ 
    vtkSmartPointer<vtkRenderer> renderer = vtkSmartPointer<vtkRenderer>::New();
    vtkSmartPointer<vtkRenderWindow> renWin = vtkSmartPointer<vtkRenderWindow>::New();
    renWin->AddRenderer(renderer);
    renWin->SetSize(800, 800);

    // Add Actor to the renderer
    renderer->AddActor(dcmActor);      // DICOM actor
    renderer->AddActor(contourActor);  // 等值线 actor
    renderer->AddActor(doseActor);     // 剂量云图 actor

    /*
    * 设置色标 scalerBar
    */
    vtkSmartPointer<vtkScalarBarActor> scalarBar = vtkSmartPointer<vtkScalarBarActor>::New();
    scalarBar->SetLookupTable(lookupTable);
    scalarBar->SetNumberOfLabels(5);
    scalarBar->SetTitle("Neutron flux(n/cm^2/s)");
    renderer->AddActor(scalarBar);

    /*
    * 渲染窗口交互器 Render Window Interactor （interactor）
    */
    vtkSmartPointer<vtkRenderWindowInteractor> interactor = vtkSmartPointer<vtkRenderWindowInteractor>::New();
    renWin->SetInteractor(interactor);

    // Start the visualization
    renderer->ResetCamera();
    renWin->Render();
    interactor->Start();

    /*
    * 释放二维数组ddata内存
    */
    for (int i = 0; i < dim; i++) {
        delete[] ddata[i];
    }
    delete[] ddata;

    return EXIT_SUCCESS;
}
